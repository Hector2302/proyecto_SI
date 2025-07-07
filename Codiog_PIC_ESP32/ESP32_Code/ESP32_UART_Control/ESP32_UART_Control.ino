/*
 * Sistema anti-incendios optimizado con Nuevo Sistema de Historial
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
const unsigned int SENSOR_PUBLISH_INTERVAL = 1000;
const unsigned int COMMAND_CHECK_INTERVAL = 2000;
const unsigned int RECONNECT_INTERVAL = 30000;

// Variables de sensores (recibidas del PIC)
float temperature = 0.0;
bool flame_detected = false;
float flame_intensity = 0.0;
float co_ppm = 0.0;
float flow_rate = 0.0;
float total_flow = 0.0;
bool pump_active = false;
bool alarm_active = false;
bool trigger_test = false;
bool shutdown_system = false;

// Estados previos para detección de cambios
bool last_trigger_test = false;
bool last_shutdown_system = false;
bool system_initialized = false;

// Variables globales adicionales
unsigned long lastHistoryClean = 0;
const unsigned int HISTORY_CLEAN_INTERVAL = 5000; // 5 segundos

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  PIC_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.println("ESP32 Fire System Optimized - New History System");
  
  setupWiFi();
  if(wifiConnected) {
    setupFirebase();
    configTime(-6*3600, 0, "pool.ntp.org"); // GMT-6 para México
    blinkLED(3, 300);
    Serial.println("System ready");
    
    // Inicializar comandos en Firebase
    Firebase.RTDB.setBool(&fbdo, "/commands/trigger_test", false);
    Firebase.RTDB.setBool(&fbdo, "/commands/shutdown_system", false);
  }
}

void loop() {
  handleWiFiReconnection();
  handleFirebaseReconnection();
  processPICData();
  publishSensorData();
  checkFirebaseCommands();
  cleanOldHistory(); // Agregar limpieza de historial
  delay(10);
}

void setupWiFi() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  
  if(!wm.autoConnect("FireSystem-AP", "configpass")) {
    Serial.println("Failed to connect, restarting...");
    delay(3000);
    ESP.restart();
  }
  
  wifiConnected = true;
  Serial.print("WiFi connected: ");
  Serial.println(WiFi.localIP());
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
    Serial.println("\nFirebase connected");
    Firebase.RTDB.setString(&fbdo, "/system/status", "online");
  } else {
    Serial.println("\nFirebase connection failed");
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
  // Parser JSON simple sin ArduinoJson
  char* ptr;
  
  // Extraer temperatura
  ptr = strstr(jsonData, "\"t\":");
  if(ptr) temperature = atof(ptr + 4);
  
  // Extraer flame_detected
  ptr = strstr(jsonData, "\"fd\":");
  if(ptr) flame_detected = (*(ptr + 5) == '1');
  
  // Extraer flame_intensity
  ptr = strstr(jsonData, "\"fi\":");
  if(ptr) flame_intensity = atof(ptr + 5);
  
  // Extraer co_ppm
  ptr = strstr(jsonData, "\"co\":");
  if(ptr) co_ppm = atof(ptr + 5);
  
  // Extraer flow_rate
  ptr = strstr(jsonData, "\"fr\":");
  if(ptr) flow_rate = atof(ptr + 5);
  
  // Extraer total_flow
  ptr = strstr(jsonData, "\"tf\":");
  if(ptr) total_flow = atof(ptr + 5);
  
  // Extraer pump_active
  ptr = strstr(jsonData, "\"p\":");
  if(ptr) pump_active = (*(ptr + 4) == '1');
  
  // Extraer alarm_active
  ptr = strstr(jsonData, "\"a\":");
  if(ptr) alarm_active = (*(ptr + 4) == '1');
  
  // Extraer trigger_test
  ptr = strstr(jsonData, "\"test\":");
  if(ptr) trigger_test = (*(ptr + 7) == '1');
  
  // Extraer shutdown_system
  ptr = strstr(jsonData, "\"shutdown\":");
  if(ptr) {
    bool previous_shutdown = shutdown_system;
    shutdown_system = (*(ptr + 11) == '1');
    
    if (previous_shutdown != shutdown_system) {
      Firebase.RTDB.setString(&fbdo, "/system/status", shutdown_system ? "standby" : "online");
    }
  }
  
  // Verificar si el sistema está en estado óptimo para generar historial inicial
  if(!system_initialized && !alarm_active && !shutdown_system && !trigger_test) {
    saveSystemStartHistory();
    system_initialized = true;
  }
  
  Serial.printf("Sensors: T=%.1fC, Flame=%d(%.1f%%), CO=%.1fppm, Flow=%.2fL/min, Total=%.2fL, Pump=%d, Alarm=%d\n",
               temperature, flame_detected, flame_intensity, co_ppm, flow_rate, 
               total_flow, pump_active, alarm_active);
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
  time_t now;
  time(&now);
  struct tm timeinfo;
  char timestamp[20];
  
  if(localtime_r(&now, &timeinfo)) {
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
  } else {
    strcpy(timestamp, "time-error");
  }
  
  // Determinar categoría y crear path
  String category = "";
  String path = "";
  
  if(strcmp(event_type, "fire_start") == 0 || strcmp(event_type, "fire_end") == 0) {
    category = "fire";
    path = "/history/fire/" + String(timestamp).substring(0,10) + "_" + String(millis());
  }
  else if(strcmp(event_type, "test_start") == 0 || strcmp(event_type, "test_end") == 0) {
    category = "test";
    path = "/history/test/" + String(timestamp).substring(0,10) + "_" + String(millis());
  }
  else if(strcmp(event_type, "shutdown") == 0 || strcmp(event_type, "resume") == 0 || strcmp(event_type, "system_start") == 0) {
    category = "status";
    path = "/history/status/" + String(timestamp).substring(0,10) + "_" + String(millis());
  }
  else {
    // Categoría por defecto
    category = "status";
    path = "/history/status/" + String(timestamp).substring(0,10) + "_" + String(millis());
  }
  
  // Crear FirebaseJson object
  FirebaseJson json;
  json.set("event_type", String(event_type));
  json.set("timestamp", String(timestamp));
  json.set("category", category);
  
  // Procesar diferentes tipos de eventos
  if(strcmp(event_type, "fire_start") == 0) {
    // Extraer qué sensor detectó el incendio
    ptr = strstr(jsonData, "\"sensor\":\"");
    if(ptr) {
      ptr += 10;
      char sensor[20] = "";
      int i = 0;
      while(*ptr != '"' && i < 19) {
        sensor[i++] = *ptr++;
      }
      sensor[i] = '\0';
      json.set("trigger_sensor", String(sensor));
    }
    
    json.set("initial_temp", temperature);
    json.set("initial_flame", flame_intensity);
    json.set("initial_co", co_ppm);
  }
  else if(strcmp(event_type, "fire_end") == 0) {
    // Extraer duración y datos finales
    ptr = strstr(jsonData, "\"duration\":");
    if(ptr) {
      json.set("duration_seconds", atol(ptr + 11));
    }
    
    json.set("final_temp", temperature);
    json.set("final_flame", flame_intensity);
    json.set("final_co", co_ppm);
    
    ptr = strstr(jsonData, "\"water\":");
    if(ptr) {
      json.set("water_used", atof(ptr + 8));
    }
  }
  else if(strcmp(event_type, "test_start") == 0) {
    json.set("test_duration", "10_seconds");
    json.set("initial_temp", temperature);
    json.set("initial_flame", flame_intensity);
    json.set("initial_co", co_ppm);
  }
  else if(strcmp(event_type, "test_end") == 0) {
    json.set("final_temp", temperature);
    json.set("final_flame", flame_intensity);
    json.set("final_co", co_ppm);
    
    ptr = strstr(jsonData, "\"water\":");
    if(ptr) {
      json.set("water_used", atof(ptr + 8));
    }
    
    // Resetear trigger_test cuando la prueba termina
    Firebase.RTDB.setBool(&fbdo, "/commands/trigger_test", false);
    Serial.println("Test completed - trigger_test reset to false");
    last_trigger_test = false;
  }
  else if(strcmp(event_type, "shutdown") == 0) {
    json.set("executed_by", "admin");
    json.set("system_temp", temperature);
    json.set("system_co", co_ppm);
  }
  else if(strcmp(event_type, "resume") == 0) {
    json.set("executed_by", "admin");
    json.set("system_temp", temperature);
    json.set("system_co", co_ppm);
  }
  
  // Guardar en Firebase usando setJSON con FirebaseJson object
  if(Firebase.RTDB.setJSON(&fbdo, path, &json)) {
    Serial.println("History saved in category [" + category + "]: " + path + " - " + String(event_type));
  } else {
    Serial.println("Failed to save history: " + fbdo.errorReason());
  }
}

void saveSystemStartHistory() {
  time_t now;
  time(&now);
  struct tm timeinfo;
  char timestamp[20];
  
  if(localtime_r(&now, &timeinfo)) {
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
  } else {
    strcpy(timestamp, "time-error");
  }
  
  // Guardar en categoría status
  String path = "/history/status/" + String(timestamp).substring(0,10) + "_system_start";
  
  // Crear FirebaseJson object
  FirebaseJson json;
  json.set("event_type", "system_start");
  json.set("timestamp", String(timestamp));
  json.set("category", "status");
  json.set("system_status", "optimal");
  json.set("initial_temp", temperature);
  json.set("initial_co", co_ppm);
  json.set("wifi_connected", true);
  json.set("firebase_connected", true);
  
  // Usar setJSON con FirebaseJson object
  if(Firebase.RTDB.setJSON(&fbdo, path, &json)) {
    Serial.println("System start history saved in [status]: " + path);
  }
}

// Función de limpieza actualizada para las tres categorías
void cleanOldHistorySimple() {
  if(!wifiConnected || !firebaseConnected) return;
  if(millis() - lastHistoryClean < HISTORY_CLEAN_INTERVAL) return;
  
  lastHistoryClean = millis();
  
  // Limpiar cada categoría por separado
  if(Firebase.RTDB.deleteNode(&fbdo, "/history/fire")) {
    Serial.println("Fire history cleared after 5 seconds");
  }
  
  if(Firebase.RTDB.deleteNode(&fbdo, "/history/test")) {
    Serial.println("Test history cleared after 5 seconds");
  }
  
  if(Firebase.RTDB.deleteNode(&fbdo, "/history/status")) {
    Serial.println("Status history cleared after 5 seconds");
  }
}

// Función alternativa para limpiar solo una categoría específica
void cleanSpecificCategory(String category) {
  if(!wifiConnected || !firebaseConnected) return;
  
  String path = "/history/" + category;
  if(Firebase.RTDB.deleteNode(&fbdo, path)) {
    Serial.println(category + " history cleared");
  } else {
    Serial.println("Failed to clear " + category + " history: " + fbdo.errorReason());
  }
}

void publishSensorData() {
  if(!wifiConnected || !firebaseConnected) return;
  if(millis() - lastSensorPublish < SENSOR_PUBLISH_INTERVAL) return;
  
  time_t now;
  time(&now);
  struct tm timeinfo;
  char timestamp[20];
  
  if(localtime_r(&now, &timeinfo)) {
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
  } else {
    strcpy(timestamp, "time-error");
  }
  
  // Crear FirebaseJson object
  FirebaseJson json;
  json.set("timestamp", String(timestamp));
  
  // Crear objetos anidados
  FirebaseJson sensors;
  sensors.set("temperature", temperature);
  sensors.set("flame_detected", flame_detected);
  sensors.set("flame_intensity", flame_intensity);
  sensors.set("co_ppm", co_ppm);
  
  FirebaseJson flow;
  flow.set("current_rate", flow_rate);
  flow.set("total", total_flow);
  
  FirebaseJson actuators;
  actuators.set("pump_active", pump_active);
  actuators.set("alarm_active", alarm_active);
  
  FirebaseJson status;
  status.set("fire_alarm", alarm_active);
  
  // Agregar objetos anidados al JSON principal
  json.set("sensors", sensors);
  json.set("flow", flow);
  json.set("actuators", actuators);
  json.set("status", status);
  
  // Usar setJSON con FirebaseJson object
  if(Firebase.RTDB.setJSON(&fbdo, "/sensor_data", &json)) {
    Serial.println("Data published to Firebase");
  } else {
    Serial.println("Firebase error: " + fbdo.errorReason());
  }
  
  lastSensorPublish = millis();
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
      Serial.println("Sent TEST command to PIC");
      logCommand("trigger_test", "admin");
    }
    last_trigger_test = current_trigger;
  }
  
  // Leer comando shutdown_system
  if(Firebase.RTDB.getBool(&fbdo, "/commands/shutdown_system")) {
    bool current_shutdown = fbdo.to<bool>();
    
    if(current_shutdown != last_shutdown_system) {
      if(current_shutdown) {
        PIC_SERIAL.write('S');
        Serial.println("Sent SHUTDOWN command to PIC");
        Firebase.RTDB.setString(&fbdo, "/system/status", "standby");
      } else {
        PIC_SERIAL.write('R');
        Serial.println("Sent RESUME command to PIC");
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
  
  // Crear FirebaseJson object
  FirebaseJson json;
  json.set("type", command_type);
  json.set("author", author);
  json.set("timestamp", String(timestamp));
  
  // Usar setJSON con FirebaseJson object
  Firebase.RTDB.setJSON(&fbdo, "/commands/last_command", &json);
}

void handleWiFiReconnection() {
  if(WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  
  digitalWrite(LED_BUILTIN, LOW);
  
  if(millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
    Serial.println("Reconnecting to WiFi...");
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
    Serial.println("Reconnecting to Firebase...");
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

void cleanOldHistory() {
  if(!wifiConnected || !firebaseConnected) return;
  if(millis() - lastHistoryClean < HISTORY_CLEAN_INTERVAL) return;
  
  lastHistoryClean = millis();
  
  // Obtener timestamp actual
  time_t now;
  time(&now);
  struct tm timeinfo;
  
  if(localtime_r(&now, &timeinfo)) {
    // Calcular timestamp de hace 5 segundos
    time_t cutoff_time = now - 5;
    struct tm cutoff_timeinfo;
    
    if(localtime_r(&cutoff_time, &cutoff_timeinfo)) {
      char cutoff_timestamp[20];
      strftime(cutoff_timestamp, sizeof(cutoff_timestamp), "%Y-%m-%d %H:%M:%S", &cutoff_timeinfo);
      
      // Obtener todos los eventos del historial
      if(Firebase.RTDB.getJSON(&fbdo, "/history")) {
        FirebaseJson json;
        json.setJsonData(fbdo.to<String>().c_str());
        
        FirebaseJsonData jsonData;
        size_t len = json.iteratorBegin();
        String key, value = "";
        int type = 0;
        
        // Iterar a través de todos los eventos
        for(size_t i = 0; i < len; i++) {
          json.iteratorGet(i, type, key, value);
          
          // Verificar si el evento es más viejo que 5 segundos
          if(key.length() > 0) {
            // Obtener el timestamp del evento
            String eventPath = "/history/" + key + "/timestamp";
            if(Firebase.RTDB.getString(&fbdo, eventPath)) {
              String eventTimestamp = fbdo.to<String>();
              
              // Comparar timestamps (formato: "YYYY-MM-DD HH:MM:SS")
              if(eventTimestamp.compareTo(String(cutoff_timestamp)) < 0) {
                // Eliminar evento viejo
                String deleteEventPath = "/history/" + key;
                if(Firebase.RTDB.deleteNode(&fbdo, deleteEventPath)) {
                  Serial.println("Deleted old history event: " + key);
                } else {
                  Serial.println("Failed to delete event: " + fbdo.errorReason());
                }
              }
            }
          }
        }
        json.iteratorEnd();
      }
    }
  }
}

// Remove this duplicate function definition (lines 627-639):
// void cleanOldHistorySimple() {
//   if(!wifiConnected || !firebaseConnected) return;
//   if(millis() - lastHistoryClean < HISTORY_CLEAN_INTERVAL) return;
//   
//   lastHistoryClean = millis();
//   
//   // Simplemente eliminar todo el nodo de historial cada 5 segundos
//   if(Firebase.RTDB.deleteNode(&fbdo, "/history")) {
//     Serial.println("History cleared after 5 seconds");
//   } else {
//     Serial.println("Failed to clear history: " + fbdo.errorReason());
//   }
// }