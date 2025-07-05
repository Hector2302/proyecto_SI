/*
 * ESP32_UART_Control.ino - Comunicación UART ESP32-PIC18F4550 con Firebase
 */

#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Definiciones básicas
#define RX_PIN 16
#define TX_PIN 17
#define LED_BUILTIN 2
#define CMD_LED_ON '1'
#define CMD_LED_OFF '0'
#define CMD_LED_AUTO 'A'
#define PIC_SERIAL Serial2

// Firebase config
#define API_KEY "AIzaSyBTAH1REzAs4EGt2K6_c6KcA85dBXzAArQ"
#define DATABASE_URL "https://firefighting-system-fed2c-default-rtdb.firebaseio.com/"

// Objetos Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variables globales
char receivedData[32];
char command = '0';
char lastFirebaseCommand = '0';
bool wifiConnected = false;
bool firebaseConnected = false;
unsigned long lastFirebaseCheck = 0;
const unsigned int FIREBASE_CHECK_INTERVAL = 2000;

void setup() {
  // Inicializar puerto serial para monitor
  Serial.begin(115200);
  
  // Configurar LED integrado
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  // Inicializar puerto serial para comunicación con PIC
  PIC_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  
  // Mensaje de inicio
  Serial.println(F("ESP32 iniciado"));
  
  // Configurar WiFi y Firebase
  setupWiFi();
  if(wifiConnected) {
    setupFirebase();
    blinkLED(5, 200);
    
    Serial.println(F("Listo! Comandos: 1=ON, 0=OFF, A=Auto"));
  }
}

void loop() {
  // Reconectar WiFi si es necesario
  if(WiFi.status() != WL_CONNECTED && wifiConnected) {
    wifiConnected = false;
    setupWiFi();
  }
  
  // Verificar comandos desde Firebase
  if(wifiConnected && firebaseConnected) {
    checkFirebaseCommands();
  }
  
  // Procesar comandos del monitor serial
  if (Serial.available() > 0) {
    command = Serial.read();
    
    // Convertir 'a' minúscula a mayúscula
    if (command == 'a') command = CMD_LED_AUTO;
    
    // Procesar comando válido
    if (command == CMD_LED_ON || command == CMD_LED_OFF || command == CMD_LED_AUTO) {
      sendCommandToPIC(command);
      
      if(wifiConnected && firebaseConnected) {
        updateFirebaseCommand(command);
      }
    }
    
    // Limpiar buffer
    while(Serial.available()) Serial.read();
  }
  
  // Procesar datos del PIC
  static uint8_t dataIndex = 0;
  while (PIC_SERIAL.available() > 0) {
    char inChar = (char)PIC_SERIAL.read();
    if (inChar == '\n' || dataIndex >= sizeof(receivedData)-1) {
      receivedData[dataIndex] = 0; // Null-terminar
      dataIndex = 0;
    } else {
      receivedData[dataIndex++] = inChar;
    }
  }
  
  delay(10);
}

// Configurar WiFi
void setupWiFi() {
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180); // 3 minutos
  
  if(!wifiManager.autoConnect("ESP32-LED-Control")) {
    ESP.restart();
  }
  
  wifiConnected = true;
  Serial.print(F("WiFi: "));
  Serial.println(WiFi.SSID());
}

// Configurar Firebase
void setupFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  
  // Agregar estas líneas para la autenticación
  auth.user.email = "playertrap230203@gmail.com"; // Reemplace con su email registrado en Firebase
  auth.user.password = "H023020am";     // Reemplace con su contraseña
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  // Esperar conexión
  uint8_t attempts = 0;
  while (!Firebase.ready() && attempts < 30) {
    delay(100);
    attempts++;
  }
  
  if (Firebase.ready()) {
    firebaseConnected = true;
    Firebase.RTDB.setString(&fbdo, "/led_control/command", "0");
    Firebase.RTDB.setString(&fbdo, "/led_control/status", "ON");
  }
}

// Verificar comandos desde Firebase
void checkFirebaseCommands() {
  if(millis() - lastFirebaseCheck > FIREBASE_CHECK_INTERVAL) {
    lastFirebaseCheck = millis();
    
    if(Firebase.RTDB.getString(&fbdo, "/led_control/command")) {
      String firebaseCommand = fbdo.stringData();
      
      if(firebaseCommand.length() > 0) {
        char newCommand = firebaseCommand.charAt(0);
        
        // Solo procesar si es un comando diferente y válido
        if(newCommand != lastFirebaseCommand && 
           (newCommand == CMD_LED_ON || newCommand == CMD_LED_OFF || newCommand == CMD_LED_AUTO)) {
          
          lastFirebaseCommand = newCommand;
          sendCommandToPIC(newCommand);
        }
      }
    }
  }
}

// Enviar comando al PIC
void sendCommandToPIC(char cmd) {
  PIC_SERIAL.write(cmd);
  Serial.print(F("Cmd: "));
  Serial.println(cmd);
}

// Actualizar comando en Firebase
void updateFirebaseCommand(char cmd) {
  Firebase.RTDB.setString(&fbdo, "/led_control/command", String(cmd));
}

// Parpadear LED
void blinkLED(int times, int delayMs) {
  for(int i = 0; i < times; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(delayMs);
    digitalWrite(LED_BUILTIN, LOW);
    delay(delayMs);
  }
}