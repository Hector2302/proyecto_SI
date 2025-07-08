/*
 * Sistema anti-incendios optimizado - PIC procesa todo
 */

#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include <time.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define RX_PIN 16
#define TX_PIN 17
#define LED_BUILTIN 2
#define PIC_SERIAL Serial2

#define API_KEY "AIzaSyBTAH1REzAs4EGt2K6_c6KcA85dBXzAArQ"
#define DATABASE_URL "https://firefighting-system-fed2c-default-rtdb.firebaseio.com/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

char receivedData[200];
bool wifiConnected = false;
bool firebaseConnected = false;
unsigned long lastSensorPublish = 0;
unsigned long lastCommandCheck = 0;
unsigned long lastReconnectAttempt = 0;
const unsigned int SENSOR_PUBLISH_INTERVAL = 500; // Cambiado a 0.5 segundos
const unsigned int COMMAND_CHECK_INTERVAL = 2000;
const unsigned int RECONNECT_INTERVAL = 30000;

// Variables globales para datos del sensor (recibidos del PIC)
float temperature = 0.0;
bool flame_detected = false;
float flame_intensity = 0.0;
float co_ppm = 0.0;
float flow_rate = 0.0;
float total_flow = 0.0;
bool pump_active = false;
bool alarm_active = false;
bool fire_alarm = false;  // AGREGAR: Recibido del PIC
bool trigger_test = false;
bool shutdown_system = false;

// Estados previos para detección de cambios
bool last_trigger_test = false;
bool last_shutdown_system = false;

// Variables globales adicionales
unsigned long lastHistoryClean = 0;
unsigned long lastFlowReset = 0;  // Nuevo: para reset de total_flow
const unsigned long HISTORY_CLEAN_INTERVAL = 1000; // Verificar cada segundo en lugar de cada 5 segundos
const unsigned int FLOW_RESET_INTERVAL = 30000;   // Nuevo: 30 segundos para reset de total

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  PIC_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.println("[DEBUG] ESP32 Fire System - PIC Processing Mode");
  
  setupWiFi();
  if(wifiConnected) {
    setupFirebase();
    configTime(-6*3600, 0, "pool.ntp.org");
    blinkLED(3, 300);
    Serial.println("[DEBUG] System ready");
    
    Firebase.RTDB.setBool(&fbdo, "/commands/trigger_test", false);
    Firebase.RTDB.setBool(&fbdo, "/commands/shutdown_system", false);
  }
}

void loop() {
    static unsigned long lastPublish = 0;
    unsigned long currentTime = millis();
    
    handleWiFiReconnection();
    handleFirebaseReconnection();
    
    // Procesar datos del PIC continuamente
    processPICData();
    
    // Publicar datos cada 500ms exactos
    if (currentTime - lastPublish >= 500) {
        publishSensorData();
        lastPublish = currentTime;
        Serial.println("[DATA] Published (0.5s)");
    }
    
    // Verificar comandos de Firebase
    checkFirebaseCommands();
    
    // Limpiar historial antiguo
    cleanOldHistorySimple();
    
    // ELIMINAR: resetTotalFlowPeriodically(); // El PIC maneja esto
}

void setupWiFi() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  
  if(!wm.autoConnect("FireSystem-AP", "configpass")) {
    Serial.println("[ERROR] WiFi failed, restarting...");
    delay(3000);
    ESP.restart();
  }
  
  wifiConnected = true;
  Serial.println("[DEBUG] WiFi OK: " + WiFi.localIP().toString());
  digitalWrite(LED_BUILTIN, HIGH);
}

void setupFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  
  auth.user.email = "playertrap230203@gmail.com";
  auth.user.password = "H023020am";
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  int attempts = 0;
  while(!Firebase.ready() && attempts < 15) {
    delay(500);
    attempts++;
    Serial.print(".");
  }
  
  if(Firebase.ready()) {
    firebaseConnected = true;
    Serial.println("[DEBUG] Firebase OK");
    Firebase.RTDB.setString(&fbdo, "/system/status", "online");
  } else {
    Serial.println("[ERROR] Firebase failed");
  }
}

void processPICData() {
  static uint8_t dataIndex = 0;
  
  while(PIC_SERIAL.available()) {
    char c = PIC_SERIAL.read();
    
    if(c == '\n' || dataIndex >= sizeof(receivedData)-1) {
      receivedData[dataIndex] = '\0';
      
      if(receivedData[0] == '{') {
        if(strstr(receivedData, "\"event\"") != NULL) {
          processHistoryEvent(receivedData);
        } else {
          parseSensorData(receivedData);
        }
      }
      dataIndex = 0;
    } 
    else if(c != '\r') {
      receivedData[dataIndex++] = c;
    }
  }
}

void parseSensorData(const char* jsonData) {
  // Parser simple - PIC ya procesó todo
  char* ptr;
  
  ptr = strstr(jsonData, "\"t\":");
  if(ptr) temperature = atof(ptr + 4);
  
  ptr = strstr(jsonData, "\"fd\":");
  if(ptr) flame_detected = (*(ptr + 5) == '1');
  
  ptr = strstr(jsonData, "\"fi\":");
  if(ptr) flame_intensity = atof(ptr + 5);
  
  ptr = strstr(jsonData, "\"co\":");
  if(ptr) co_ppm = atof(ptr + 5);
  
  ptr = strstr(jsonData, "\"fr\":");
  if(ptr) flow_rate = atof(ptr + 5);
  
  ptr = strstr(jsonData, "\"tf\":");
  if(ptr) total_flow = atof(ptr + 5);
  
  ptr = strstr(jsonData, "\"p\":");
  if(ptr) pump_active = (*(ptr + 4) == '1');
  
  ptr = strstr(jsonData, "\"a\":");
  if(ptr) alarm_active = (*(ptr + 4) == '1');
  
  // AGREGAR: Recibir fire_alarm del PIC
  ptr = strstr(jsonData, "\"fa\":");
  if(ptr) fire_alarm = (*(ptr + 5) == '1');
  
  ptr = strstr(jsonData, "\"test\":");
  if(ptr) {
    bool previous_test = trigger_test;
    trigger_test = (*(ptr + 7) == '1');
  }
  
  ptr = strstr(jsonData, "\"shutdown\":");
  if(ptr) {
    bool previous_shutdown = shutdown_system;
    shutdown_system = (*(ptr + 11) == '1');
    
    if (previous_shutdown != shutdown_system) {
      Firebase.RTDB.setString(&fbdo, "/system/status", shutdown_system ? "standby" : "online");
    }
  }
  
  // Log simplificado para depuración
  Serial.printf("[DATA] T=%.1f F=%d(%.1f) CO=%.1f FR=%.2f P=%d A=%d FA=%d\n",
               temperature, flame_detected, flame_intensity, co_ppm, flow_rate, pump_active, alarm_active, fire_alarm);
}

void processHistoryEvent(const char* jsonData) {
  char* ptr;
  char event_type[30] = "";
  
  // Extraer tipo de evento
  ptr = strstr(jsonData, "\"event\":\"");
  if(ptr) {
    ptr += 9;
    int i = 0;
    while(*ptr != '"' && i < 29) {
      event_type[i++] = *ptr++;
    }
    event_type[i] = '\0';
  }
  
  // Crear timestamp
  String timestamp = getFormattedTime();
  
  // Determinar categoría del evento
  String category = "status";
  if(strstr(event_type, "fire")) {
    category = "fire";
  } else if(strstr(event_type, "test")) {
    category = "test";
  }
  
  // Crear JSON del evento
  FirebaseJson eventJson;
  eventJson.set("event", String(event_type));
  eventJson.set("timestamp", timestamp);
  eventJson.set("created_millis", millis()); // Agregar timestamp en millis para limpieza
  
  // Para eventos finales (fire_end, test_end), incluir TODOS los datos de sensores
  if(strstr(event_type, "_end")) {
    // Agregar todos los datos de sensores actuales
    FirebaseJson sensorData;
    sensorData.set("temperature", temperature);
    sensorData.set("flame_detected", flame_detected);
    sensorData.set("flame_intensity", flame_intensity);
    sensorData.set("co_ppm", co_ppm);
    sensorData.set("flow_rate", flow_rate);
    sensorData.set("total_flow", total_flow);
    
    FirebaseJson actuatorData;
    actuatorData.set("pump_active", pump_active);
    actuatorData.set("alarm_active", alarm_active);
    
    FirebaseJson systemStatus;
    bool fire_alarm = flame_detected || (temperature > 40.0) || (co_ppm > 50.0);
    systemStatus.set("fire_alarm", fire_alarm);
    systemStatus.set("trigger_test", trigger_test);
    systemStatus.set("shutdown_system", shutdown_system);
    
    eventJson.set("sensor_data", sensorData);
    eventJson.set("actuator_data", actuatorData);
    eventJson.set("system_status", systemStatus);
  }
  
  // Agregar datos adicionales específicos del evento
  if(strstr(jsonData, "\"sensor\":")) {
    ptr = strstr(jsonData, "\"sensor\":\"");
    if(ptr) {
      ptr += 10;
      char sensor[20] = "";
      int i = 0;
      while(*ptr != '"' && i < 19) {
        sensor[i++] = *ptr++;
      }
      sensor[i] = '\0';
      eventJson.set("trigger_sensor", String(sensor));
    }
  }
  
  if(strstr(jsonData, "\"duration\":")) {
    ptr = strstr(jsonData, "\"duration\":");
    if(ptr) {
      ptr += 11;
      int duration = atoi(ptr);
      eventJson.set("duration_seconds", duration);
    }
  }
  
  if(strstr(jsonData, "\"water\":")) {
    ptr = strstr(jsonData, "\"water\":");
    if(ptr) {
      ptr += 8;
      float water = atof(ptr);
      eventJson.set("water_used", water);
    }
  }
  
  // Guardar en Firebase con nombre descriptivo por categoría
  String eventName = String(event_type) + "_" + String(millis());
  String path = "/system/history/" + category + "/" + eventName;
  
  if(Firebase.RTDB.setJSON(&fbdo, path.c_str(), &eventJson)) {
    Serial.println("[HISTORY] Event saved with sensor data: " + String(event_type) + " in category: " + category);
  }
}

// Función de limpieza cada 5 segundos
void cleanOldHistorySimple() {
  if(!wifiConnected || !firebaseConnected) return;
  if(millis() - lastHistoryClean < 1000) return; // Verificar cada segundo
  
  lastHistoryClean = millis();
  
  // Categorías a limpiar
  String categories[] = {"fire", "test", "status"};
  
  for(int c = 0; c < 3; c++) {
    String categoryPath = "/system/history/" + categories[c];
    
    // Obtener todas las entradas de esta categoría
    if(Firebase.RTDB.getJSON(&fbdo, categoryPath.c_str())) {
      FirebaseJson json = fbdo.to<FirebaseJson>();
      FirebaseJsonData jsonData;
      
      // Obtener todas las claves (nombres de eventos)
      size_t len = json.iteratorBegin();
      String key, value = "";
      int type = 0;
      
      for(size_t i = 0; i < len; i++) {
        json.iteratorGet(i, type, key, value);
        
        // Calcular la edad de la entrada usando millis() del nombre
        // El nombre tiene formato: "evento_millis"
        int underscorePos = key.lastIndexOf('_');
        if(underscorePos > 0) {
          String millisStr = key.substring(underscorePos + 1);
          unsigned long entryMillis = millisStr.toInt();
          unsigned long currentMillis = millis();
          
          // Si han pasado más de 5 segundos (5000ms), eliminar SOLO la entrada
          if(currentMillis - entryMillis > 5000) {
            String fullEntryPath = categoryPath + "/" + key;
            if(Firebase.RTDB.deleteNode(&fbdo, fullEntryPath.c_str())) {
              Serial.println("[DEBUG] Deleted old entry: " + key + " (age: " + String(currentMillis - entryMillis) + "ms)");
            }
          }
        }
      }
      json.iteratorEnd();
    } else {
      // Si la categoría no existe, crearla con un placeholder vacío
      String placeholderPath = categoryPath + "/placeholder";
      FirebaseJson placeholder;
      placeholder.set("info", "category_structure");
      Firebase.RTDB.setJSON(&fbdo, placeholderPath.c_str(), &placeholder);
      Serial.println("[DEBUG] Created category structure: " + categories[c]);
    }
  }
}

// Función mejorada para publicar datos del sensor
String getFormattedTime() {
  time_t now;
  time(&now);
  struct tm timeinfo;
  char timestamp[20];
  
  if(localtime_r(&now, &timeinfo)) {
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timestamp);
  } else {
    return "time-error";
  }
}

void publishSensorData() {
    // SIMPLIFICADO: Publicar siempre cada 0.5s
    FirebaseJson json;
    
    FirebaseJson actuators;
    actuators.set("alarm_active", alarm_active);
    actuators.set("pump_active", pump_active);
    
    FirebaseJson flow;
    flow.set("current_rate", flow_rate);
    flow.set("total", total_flow);
    
    FirebaseJson sensors;
    sensors.set("temperature", temperature);
    sensors.set("flame_detected", flame_detected);
    sensors.set("flame_intensity", flame_intensity);
    sensors.set("co_ppm", co_ppm);
    
    FirebaseJson status;
    status.set("fire_alarm", fire_alarm);  // Recibido del PIC
    status.set("test_mode", trigger_test);
    status.set("shutdown", shutdown_system);
    
    json.set("timestamp", getFormattedTime());
    json.set("sensors", sensors);
    json.set("actuators", actuators);
    json.set("flow", flow);
    json.set("status", status);
    
    if(firebaseConnected) {
        Firebase.RTDB.setJSON(&fbdo, "/system/sensor_data", &json);
    }
}

void checkFirebaseCommands() {
  if(!wifiConnected || !firebaseConnected) return;
  if(millis() - lastCommandCheck < COMMAND_CHECK_INTERVAL) return;
  
  lastCommandCheck = millis();
  
  // Leer comando trigger_test
  if(Firebase.RTDB.getBool(&fbdo, "/commands/trigger_test")) {
    bool current_trigger = fbdo.to<bool>();
    
    if(current_trigger && !last_trigger_test) {
      PIC_SERIAL.write('T');
      Serial.println("[CMD] TEST sent to PIC");
      logCommand("trigger_test", "admin");
    }
    
    // NUEVO: Detectar cuando el test termina en el PIC y resetear Firebase
    if(last_trigger_test && !trigger_test) {
      // El test terminó en el PIC, resetear Firebase
      Firebase.RTDB.setBool(&fbdo, "/commands/trigger_test", false);
      Serial.println("[CMD] TEST finished - Firebase reset to false");
    }
    
    last_trigger_test = current_trigger;
  }
  
  // Leer comando shutdown_system
  if(Firebase.RTDB.getBool(&fbdo, "/commands/shutdown_system")) {
    bool current_shutdown = fbdo.to<bool>();
    
    if(current_shutdown != last_shutdown_system) {
      if(current_shutdown) {
        PIC_SERIAL.write('S');
        Serial.println("[CMD] SHUTDOWN sent to PIC");
        Firebase.RTDB.setString(&fbdo, "/system/status", "standby");
      } else {
        PIC_SERIAL.write('R');
        Serial.println("[CMD] RESUME sent to PIC");
        Firebase.RTDB.setString(&fbdo, "/system/status", "online");
      }
      logCommand(current_shutdown ? "shutdown" : "resume", "admin");
    }
    last_shutdown_system = current_shutdown;
  }
}

void logCommand(String command_type, String author) {
  if(!firebaseConnected) return;
  
  time_t now;
  time(&now);
  struct tm timeinfo;
  char timestamp[20];
  if(localtime_r(&now, &timeinfo)) {
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
  } else {
    strcpy(timestamp, "time-error");
  }
  
  FirebaseJson json;
  json.set("type", command_type);
  json.set("author", author);
  json.set("timestamp", String(timestamp));
  
  Firebase.RTDB.setJSON(&fbdo, "/system/commands/last_command", &json);
}

void handleWiFiReconnection() {
  if(WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  
  digitalWrite(LED_BUILTIN, LOW);
  
  if(millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
    Serial.println("[DEBUG] WiFi reconnecting...");
    WiFi.reconnect();
    lastReconnectAttempt = millis();
    
    if(WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }
}

void handleFirebaseReconnection() {
  if(Firebase.ready()) {
    firebaseConnected = true;
    return;
  }
  
  if(millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
    Serial.println("[DEBUG] Firebase reconnecting...");
    setupFirebase();
    lastReconnectAttempt = millis();
  }
}

void blinkLED(int times, int delayMs) {
  for(int i = 0; i < times; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(delayMs);
    digitalWrite(LED_BUILTIN, LOW);
    delay(delayMs);
  }
}

// Nueva función para resetear el total_flow periódicamente
void resetTotalFlowPeriodically() {
  if(!wifiConnected || !firebaseConnected) return;
  if(millis() - lastFlowReset < FLOW_RESET_INTERVAL) return;
  
  lastFlowReset = millis();
  
  // Enviar comando de reset al PIC
  PIC_SERIAL.write('F'); // Nuevo comando para reset de flow total
  
  // También resetear la variable local
  total_flow = 0.0;
  
  Serial.println("[DEBUG] Total flow reset (30s)");
  
  // ELIMINAR: No guardar en Firebase history
  // El resto del código de guardado en Firebase se elimina
}