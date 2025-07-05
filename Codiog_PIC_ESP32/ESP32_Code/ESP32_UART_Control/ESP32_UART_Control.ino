/*
 * Sistema anti-incendios con gestión de historial - ESP32 (CORREGIDO)
 */

#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include <time.h>
#include <map>
#include <vector>
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

char receivedData[250];
bool wifiConnected = false;
bool firebaseConnected = false;
unsigned long lastSensorPublish = 0;
unsigned long lastCommandCheck = 0;
unsigned long lastReconnectAttempt = 0;
const unsigned int SENSOR_PUBLISH_INTERVAL = 1000;
const unsigned int COMMAND_CHECK_INTERVAL = 2000;
const unsigned int RECONNECT_INTERVAL = 30000;

// Variables de sensores
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

// Historial de eventos
std::map<String, unsigned long> event_history; // path -> timestamp de creación

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  PIC_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.println("ESP32 Fire System with Event History");
  
  setupWiFi();
  if(wifiConnected) {
    setupFirebase();
    configTime(0, 0, "pool.ntp.org");
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
  cleanOldEvents(); // Limpiar eventos antiguos
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
        // Verificar si es un evento
        if(strstr(receivedData, "\"event\"") != NULL) {
          processEvent(receivedData);
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
  StaticJsonDocument<300> doc;
  DeserializationError error = deserializeJson(doc, jsonData);
  
  if(error) {
    Serial.print("JSON error: ");
    Serial.println(error.c_str());
    Serial.println("Raw data: " + String(receivedData));
    return;
  }
  
  temperature = doc["t"];
  flame_detected = doc["fd"];
  flame_intensity = doc["fi"];
  co_ppm = doc["co"];
  flow_rate = doc["fr"];
  total_flow = doc["tf"];
  pump_active = doc["p"];
  alarm_active = doc["a"];
  trigger_test = doc["cmd"]["test"];
  shutdown_system = doc["cmd"]["shutdown"];
  
  Serial.printf("Sensors: T=%.1fC, Flame=%d(%.1f%%), CO=%.1fppm, Flow=%.2fL/min, Total=%.2fL, Pump=%d, Alarm=%d, Test=%d, Shutdown=%d\n",
               temperature, flame_detected, flame_intensity, co_ppm, flow_rate, 
               total_flow, pump_active, alarm_active, trigger_test, shutdown_system);
}

void processEvent(const char* jsonData) {
    StaticJsonDocument<300> doc;
    DeserializationError error = deserializeJson(doc, jsonData);
    
    if(error) {
        Serial.print("Event JSON error: ");
        Serial.println(error.c_str());
        Serial.println("Raw event data: " + String(jsonData));
        return;
    }
    
    String event_type = doc["event"].as<String>();
    unsigned long event_time = doc["time"];
    
    // Crear objeto JSON para el evento
    FirebaseJson event_json;
    event_json.set("type", event_type);
    event_json.set("timestamp", event_time);
    
    // Agregar datos adicionales para eventos finales
    if(event_type == "fire_end" || event_type == "test_end" || event_type == "shutdown_end") {
        // Extraer valores primero a variables nativas
        unsigned long start_time = doc["start_time"];
        float start_temp = doc["t0"];
        float start_fi = doc["fi0"];
        float start_co = doc["co0"];
        float water_used = doc["water"];
        
        // Ahora asignar al FirebaseJson
        event_json.set("start_time", start_time);
        event_json.set("start_temp", start_temp);
        event_json.set("start_fi", start_fi);
        event_json.set("start_co", start_co);
        event_json.set("water_used", water_used);
    }
    
    // Crear path único
    String path = "/history/event_" + String(millis());
    
    // Guardar en Firebase
    if(Firebase.RTDB.setJSON(&fbdo, path, &event_json)) {
        Serial.println("Event saved to history: " + path + " - " + event_type);
        // Registrar tiempo de creación
        event_history[path] = millis();
    } else {
        Serial.println("Failed to save event: " + fbdo.errorReason());
    }
}

void cleanOldEvents() {
    unsigned long current_time = millis();
    std::vector<String> to_erase;
    
    for (const auto& pair : event_history) {
        if (current_time - pair.second >= 10000) { // 10 segundos
            // Eliminar de Firebase
            if(Firebase.RTDB.deleteNode(&fbdo, pair.first)) {
                Serial.println("Deleted old event: " + pair.first);
            } else {
                Serial.println("Failed to delete event: " + pair.first);
            }
            to_erase.push_back(pair.first);
        }
    }
    
    // Eliminar del registro
    for (const String& key : to_erase) {
        event_history.erase(key);
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
  
  FirebaseJson json;
  json.set("timestamp", timestamp);
  json.set("sensors/temperature", temperature);
  json.set("sensors/flame_detected", flame_detected);
  json.set("sensors/flame_intensity", flame_intensity);
  json.set("sensors/co_ppm", co_ppm);
  json.set("flow/current_rate", flow_rate);
  json.set("flow/total", total_flow);
  json.set("actuators/pump_active", pump_active);
  json.set("actuators/alarm_active", alarm_active);
  json.set("status/fire_alarm", alarm_active);
  
  if(Firebase.RTDB.updateNode(&fbdo, "/sensor_data", &json)) {
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
    
    // Solo actuar si el comando cambió a true
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
      } else {
        PIC_SERIAL.write('R');
        Serial.println("Sent RESUME command to PIC");
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
  
  FirebaseJson command_json;
  command_json.set("type", command_type);
  command_json.set("author", author);
  command_json.set("timestamp", timestamp);
  
  Firebase.RTDB.setJSON(&fbdo, "/commands/last_command", &command_json);
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