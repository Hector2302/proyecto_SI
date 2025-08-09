# Documentación del Sistema ESP32 - Sistema Anti-Incendios

## Descripción General

El ESP32 actúa como **puente de comunicación** entre el PIC18F4550 y la infraestructura en la nube (Firebase). Su función principal es recibir datos del PIC, procesarlos y enviarlos a Firebase, además de gestionar comandos remotos desde la aplicación móvil.

## Función Principal

El ESP32 es el **gateway IoT** del sistema, ejecutando las siguientes tareas:

- **Comunicación UART** con el PIC18F4550
- **Conectividad WiFi** y gestión de red
- **Integración con Firebase** (Realtime Database)
- **Procesamiento de comandos remotos**
- **Gestión de historial de eventos**
- **Sincronización temporal** (NTP)
- **Notificaciones push** (FCM)

## Arquitectura del Hardware

### Microcontrolador
- **Modelo:** ESP32-WROOM-32
- **Frecuencia:** 240 MHz (dual core)
- **Memoria RAM:** 520 KB
- **Memoria Flash:** 4 MB
- **Voltaje de operación:** 3.3V
- **WiFi:** 802.11 b/g/n
- **Bluetooth:** 4.2 BR/EDR y BLE

### Configuración de Pines

| Pin | Función | Descripción |
|-----|---------|-------------|
| GPIO16 | UART2 RX | Recepción desde PIC (via convertidor) |
| GPIO17 | UART2 TX | Transmisión al PIC (via convertidor) |
| GPIO2 | LED_BUILTIN | Indicador de estado WiFi |
| 3.3V | Alimentación | Voltaje de operación |
| GND | Tierra | Referencia común |

## Convertidor de Niveles Lógicos

### Necesidad del Convertidor
El PIC opera a **5V** mientras que el ESP32 opera a **3.3V**. El convertidor M001 es esencial para:
- Proteger las entradas del ESP32
- Garantizar niveles lógicos correctos
- Mantener integridad de la comunicación

### Conexiones del Convertidor M001

**Lado HV (5V - PIC):**
- HV1 ← PIC RC6 (TX)
- HV2 → PIC RC7 (RX)
- HV ← +5V
- GND ← GND común

**Lado LV (3.3V - ESP32):**
- LV1 → ESP32 GPIO16 (RX)
- LV2 ← ESP32 GPIO17 (TX)
- LV ← +3.3V
- GND ← GND común

## Arquitectura del Software

### Librerías Utilizadas
```cpp
#include <WiFi.h>           // Conectividad WiFi
#include <WiFiManager.h>    // Configuración automática WiFi
#include <Firebase_ESP_Client.h>  // Cliente Firebase
#include <time.h>           // Sincronización temporal
```

### Configuración Firebase
```cpp
#define API_KEY "AIzaSyBTAH1REzAs4EGt2K6_c6KcA85dBXzAArQ"
#define DATABASE_URL "https://firefighting-system-fed2c-default-rtdb.firebaseio.com/"
```

### Variables Globales Principales
```cpp
// Estados de conexión
bool wifiConnected = false;
bool firebaseConnected = false;

// Datos de sensores (recibidos del PIC)
float temperature = 0.0;
bool flame_detected = false;
float flame_intensity = 0.0;
float co_ppm = 0.0;
float flow_rate = 0.0;
float total_flow = 0.0;
bool pump_active = false;
bool alarm_active = false;
bool fire_alarm = false;

// Estados de comandos
bool trigger_test = false;
bool shutdown_system = false;
```

## Funciones Principales

### 1. Configuración WiFi
```cpp
void setupWiFi() {
    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    
    if(!wm.autoConnect("FireSystem-AP", "configpass")) {
        ESP.restart();
    }
    
    wifiConnected = true;
    digitalWrite(LED_BUILTIN, HIGH);
}
```

**Características:**
- Portal cautivo para configuración
- SSID: "FireSystem-AP"
- Password: "configpass"
- Timeout: 180 segundos
- Reconexión automática

### 2. Configuración Firebase
```cpp
void setupFirebase() {
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    
    auth.user.email = "playertrap230203@gmail.com";
    auth.user.password = "H023020am";
    
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
}
```

### 3. Procesamiento de Datos del PIC
```cpp
void processPICData() {
    while(PIC_SERIAL.available()) {
        char c = PIC_SERIAL.read();
        
        if(c == '\n') {
            if(receivedData[0] == '{') {
                if(strstr(receivedData, "\"event\"") != NULL) {
                    processHistoryEvent(receivedData);
                } else {
                    parseSensorData(receivedData);
                }
            }
        }
    }
}
```

### 4. Parser de Datos JSON
```cpp
void parseSensorData(const char* jsonData) {
    char* ptr;
    
    ptr = strstr(jsonData, "\"t\":");
    if(ptr) temperature = atof(ptr + 4);
    
    ptr = strstr(jsonData, "\"fd\":");
    if(ptr) flame_detected = (*(ptr + 5) == '1');
    
    // ... más campos
}
```

## Protocolo de Comunicación

### Datos Recibidos del PIC

**Formato JSON de Sensores:**
```json
{
  "t": 25.5,
  "fd": 1,
  "fi": 45.2,
  "co": 25.0,
  "fr": 2.5,
  "tf": 15.8,
  "p": 1,
  "a": 1,
  "fa": 1,
  "cmd": {
    "test": 0,
    "shutdown": 0
  }
}
```

**Formato JSON de Eventos:**
```json
{
  "event": "fire_start",
  "time": 12345,
  "sensor": "flame_sensor"
}
```

### Comandos Enviados al PIC

| Comando | Carácter | Descripción |
|---------|----------|-------------|
| Test | 'T' | Iniciar prueba del sistema |
| Shutdown | 'S' | Apagar sistema |
| Resume | 'R' | Reanudar operación |
| Flow Reset | 'F' | Reset contador de flujo |

## Integración con Firebase

### Estructura de Datos en Firebase

firebase-root/
├── system/
│   ├── sensor_data/          # Datos en tiempo real
│   ├── status                # Estado del sistema
│   ├── history/              # Historial de eventos
│   │   ├── fire/            # Eventos de incendio
│   │   ├── test/            # Eventos de prueba
│   │   └── status/          # Cambios de estado
│   └── commands/            # Comandos remotos
│       ├── trigger_test
│       ├── shutdown_system
│       └── last_command
└── notifications/           # Sistema de notificaciones

---

# Comenzando con la Documentación Técnica y Detallada

## Análisis Completo del Código Fuente ESP32

### Configuración Inicial del Sistema

```cpp
// Definiciones de hardware y comunicación
#define RX_PIN 16           // Pin GPIO16 para recepción UART2
#define TX_PIN 17           // Pin GPIO17 para transmisión UART2
#define LED_BUILTIN 2       // Pin GPIO2 para LED indicador
#define PIC_SERIAL Serial2  // Alias para puerto serie 2

// Credenciales de Firebase
#define API_KEY "AIzaSyBTAH1REzAs4EGt2K6_c6KcA85dBXzAArQ"
#define DATABASE_URL "https://firefighting-system-fed2c-default-rtdb.firebaseio.com/"

// Objetos Firebase
FirebaseData fbdo;    // Objeto para operaciones de base de datos
FirebaseAuth auth;    // Objeto para autenticación
FirebaseConfig config; // Objeto para configuración
```

### Variables de Control de Tiempo

```cpp
// Control de intervalos de tiempo
unsigned long lastSensorPublish = 0;
unsigned long lastCommandCheck = 0;
unsigned long lastReconnectAttempt = 0;

// Intervalos optimizados para el sistema
const unsigned int SENSOR_PUBLISH_INTERVAL = 500;  // 0.5 segundos para datos
const unsigned int COMMAND_CHECK_INTERVAL = 2000;  // 2 segundos para comandos
const unsigned int RECONNECT_INTERVAL = 30000;     // 30 segundos para reconexión
```

**Explicación de intervalos:**
- **500ms para sensores:** Balance entre tiempo real y carga de red
- **2000ms para comandos:** Suficiente para respuesta de usuario
- **30000ms para reconexión:** Evita sobrecarga en caso de fallas

### Variables de Estado del Sistema

```cpp
// Variables globales para datos del sensor (recibidos del PIC)
float temperature = 0.0;        // Temperatura en °C
bool flame_detected = false;    // Estado de detección de llama
float flame_intensity = 0.0;    // Intensidad de llama en porcentaje
float co_ppm = 0.0;            // Concentración de CO en ppm
float flow_rate = 0.0;         // Flujo instantáneo en L/min
float total_flow = 0.0;        // Flujo total acumulado en litros
bool pump_active = false;       // Estado de la bomba
bool alarm_active = false;      // Estado de la alarma
bool fire_alarm = false;        // Estado de alarma de incendio
bool trigger_test = false;      // Estado de prueba activa
bool shutdown_system = false;   // Estado de apagado del sistema

// Estados previos para detección de cambios
bool last_trigger_test = false;
bool last_shutdown_system = false;

// Variables para gestión de historial y flujo
unsigned long lastHistoryClean = 0;
unsigned long lastFlowReset = 0;
const unsigned long HISTORY_CLEAN_INTERVAL = 1000;  // Limpiar cada segundo
const unsigned int FLOW_RESET_INTERVAL = 30000;     // Reset cada 30 segundos
```

### Función Setup - Inicialización del Sistema

```cpp
void setup() {
    // Inicialización de comunicación serie
    Serial.begin(115200);        // Monitor serie para debug
    
    // Configuración de pines
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW); // LED apagado inicialmente
    
    // Inicialización de UART2 para comunicación con PIC
    PIC_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial.println("[DEBUG] ESP32 Fire System - PIC Processing Mode");
    
    // Configuración de conectividad
    setupWiFi();
    if(wifiConnected) {
        setupFirebase();
        
        // Configuración de tiempo (NTP)
        configTime(-6*3600, 0, "pool.ntp.org"); // UTC-6 (México)
        
        // Indicación visual de sistema listo
        blinkLED(3, 300);
        Serial.println("[DEBUG] System ready");
        
        // Inicialización de comandos en Firebase
        Firebase.RTDB.setBool(&fbdo, "/commands/trigger_test", false);
        Firebase.RTDB.setBool(&fbdo, "/commands/shutdown_system", false);
    }
}
```

**Configuración UART2:**
- **Velocidad:** 9600 baudios (compatible con PIC)
- **Formato:** 8 bits de datos, sin paridad, 1 bit de stop
- **Pines:** GPIO16 (RX), GPIO17 (TX)

### Bucle Principal del Sistema

```cpp
void loop() {
    static unsigned long lastPublish = 0;
    unsigned long currentTime = millis();
    
    // Gestión de conectividad
    handleWiFiReconnection();
    handleFirebaseReconnection();
    
    // Procesamiento continuo de datos del PIC
    processPICData();
    
    // Publicación de datos cada 500ms exactos
    if (currentTime - lastPublish >= 500) {
        publishSensorData();
        lastPublish = currentTime;
    }
    
    // Verificación de comandos de Firebase
    checkFirebaseCommands();
    
    // Limpieza de historial antiguo
    cleanOldHistorySimple();
    
    // Reset automático de flujo después de emergencias
    handleEmergencyFlowReset();
}
```

### Configuración WiFi con WiFiManager

```cpp
void setupWiFi() {
    WiFiManager wm;
    
    // Configuración del portal cautivo
    wm.setConfigPortalTimeout(180); // 3 minutos de timeout
    
    // Intento de conexión automática o portal de configuración
    if(!wm.autoConnect("FireSystem-AP", "configpass")) {
        Serial.println("[ERROR] WiFi failed, restarting...");
        delay(3000);
        ESP.restart(); // Reinicio si falla la conexión
    }
    
    // Confirmación de conexión exitosa
    wifiConnected = true;
    Serial.println("[DEBUG] WiFi OK: " + WiFi.localIP().toString());
    digitalWrite(LED_BUILTIN, HIGH); // LED encendido = WiFi conectado
}
```

**Características del WiFiManager:**
- **Portal cautivo:** Permite configuración sin hardcodear credenciales
- **SSID del AP:** "FireSystem-AP"
- **Password del AP:** "configpass"
- **Timeout:** 180 segundos para configuración
- **Fallback:** Reinicio automático si falla

### Configuración de Firebase

```cpp
void setupFirebase() {
    // Configuración de API y base de datos
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    config.token_status_callback = tokenStatusCallback; // Callback para tokens
    
    // Autenticación con email y password
    auth.user.email = "playertrap230203@gmail.com";
    auth.user.password = "H023020am";
    
    // Inicialización de Firebase
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true); // Reconexión automática de WiFi
    
    // Espera de conexión con timeout
    int attempts = 0;
    while(!Firebase.ready() && attempts < 15) {
        delay(500);
        attempts++;
        Serial.print(".");
    }
    
    // Verificación de conexión exitosa
    if(Firebase.ready()) {
        firebaseConnected = true;
        Serial.println("[DEBUG] Firebase OK");
        Firebase.RTDB.setString(&fbdo, "/system/status", "online");
    } else {
        Serial.println("[ERROR] Firebase failed");
    }
}
```

### Procesamiento de Datos del PIC

```cpp
void processPICData() {
    static uint8_t dataIndex = 0; // Índice para buffer de recepción
    
    // Procesamiento carácter por carácter
    while(PIC_SERIAL.available()) {
        char c = PIC_SERIAL.read();
        
        // Detección de fin de línea o buffer lleno
        if(c == '\n' || dataIndex >= sizeof(receivedData)-1) {
            receivedData[dataIndex] = '\0'; // Terminador de cadena
            
            // Procesamiento solo de datos JSON válidos
            if(receivedData[0] == '{') {
                // Diferenciación entre eventos y datos de sensores
                if(strstr(receivedData, "\"event\"") != NULL) {
                    processHistoryEvent(receivedData);
                } else {
                    parseSensorData(receivedData);
                }
            }
            dataIndex = 0; // Reset del índice
        } 
        else if(c != '\r') { // Ignorar retorno de carro
            receivedData[dataIndex++] = c;
        }
    }
}
```

**Protocolo de recepción:**
1. **Lectura carácter por carácter** para evitar pérdida de datos
2. **Buffer de 200 caracteres** para mensajes completos
3. **Detección de JSON** por carácter inicial '{'
4. **Diferenciación automática** entre datos y eventos
5. **Protección contra overflow** del buffer

### Parser de Datos de Sensores

```cpp
void parseSensorData(const char* jsonData) {
    char* ptr; // Puntero para búsqueda en cadena
    
    // Extracción de temperatura
    ptr = strstr(jsonData, "\"t\":");
    if(ptr) temperature = atof(ptr + 4);
    
    // Extracción de detección de llama (booleano)
    ptr = strstr(jsonData, "\"fd\":");
    if(ptr) flame_detected = (*(ptr + 5) == '1');
    
    // Extracción de intensidad de llama
    ptr = strstr(jsonData, "\"fi\":");
    if(ptr) flame_intensity = atof(ptr + 5);
    
    // Extracción de concentración de CO
    ptr = strstr(jsonData, "\"co\":");
    if(ptr) co_ppm = atof(ptr + 5);
    
    // Extracción de flujo instantáneo
    ptr = strstr(jsonData, "\"fr\":");
    if(ptr) flow_rate = atof(ptr + 5);
    
    // Extracción de flujo total
    ptr = strstr(jsonData, "\"tf\":");
    if(ptr) total_flow = atof(ptr + 5);
    
    // Extracción de estado de bomba
    ptr = strstr(jsonData, "\"p\":");
    if(ptr) pump_active = (*(ptr + 4) == '1');
    
    // Extracción de estado de alarma
    ptr = strstr(jsonData, "\"a\":");
    if(ptr) alarm_active = (*(ptr + 4) == '1');
    
    // Extracción de alarma de incendio
    ptr = strstr(jsonData, "\"fa\":");
    if(ptr) fire_alarm = (*(ptr + 5) == '1');
    
    // Extracción de estado de prueba
    ptr = strstr(jsonData, "\"test\":");
    if(ptr) {
        bool previous_test = trigger_test;
        trigger_test = (*(ptr + 7) == '1');
    }
    
    // Extracción de estado de apagado
    ptr = strstr(jsonData, "\"shutdown\":");
    if(ptr) {
        bool previous_shutdown = shutdown_system;
        shutdown_system = (*(ptr + 11) == '1');
        
        // Actualización de estado en Firebase si hay cambio
        if (previous_shutdown != shutdown_system) {
            Firebase.RTDB.setString(&fbdo, "/system/status", 
                                   shutdown_system ? "standby" : "online");
        }
    }
}
```

**Técnica de parsing:**
- **Búsqueda de patrones:** `strstr()` para localizar campos JSON
- **Conversión de tipos:** `atof()` para flotantes, comparación para booleanos
- **Offset calculado:** Posición exacta después del nombre del campo
- **Detección de cambios:** Comparación con estados previos

### Procesamiento de Eventos Históricos

```cpp
void processHistoryEvent(const char* jsonData) {
    char* ptr;
    char event_type[30] = "";
    
    // Extracción del tipo de evento
    ptr = strstr(jsonData, "\"event\":\"");
    if(ptr) {
        ptr += 9; // Saltar '"event":"'
        int i = 0;
        while(*ptr != '"' && i < 29) {
            event_type[i++] = *ptr++;
        }
        event_type[i] = '\0';
    }
    
    // Creación de timestamp formateado
    String timestamp = getFormattedTime();
    
    // Determinación de categoría del evento
    String category = "status";
    if(strstr(event_type, "fire")) {
        category = "fire";
    } else if(strstr(event_type, "test")) {
        category = "test";
    }
    
    // Creación del objeto JSON del evento
    FirebaseJson eventJson;
    eventJson.set("event", String(event_type));
    eventJson.set("timestamp", timestamp);
    eventJson.set("created_millis", millis()); // Para limpieza automática
    
    // Para eventos finales, incluir todos los datos de sensores
    if(strstr(event_type, "_end")) {
        // Datos de sensores
        FirebaseJson sensorData;
        sensorData.set("temperature", temperature);
        sensorData.set("flame_detected", flame_detected);
        sensorData.set("flame_intensity", flame_intensity);
        sensorData.set("co_ppm", co_ppm);
        sensorData.set("flow_rate", flow_rate);
        sensorData.set("total_flow", total_flow);
        
        // Datos de actuadores
        FirebaseJson actuatorData;
        actuatorData.set("pump_active", pump_active);
        actuatorData.set("alarm_active", alarm_active);
        
        // Estado del sistema
        FirebaseJson systemStatus;
        bool fire_alarm_calc = flame_detected || (temperature > 40.0) || (co_ppm > 50.0);
        systemStatus.set("fire_alarm", fire_alarm_calc);
        systemStatus.set("trigger_test", trigger_test);
        systemStatus.set("shutdown_system", shutdown_system);
        
        // Agregar al evento
        eventJson.set("sensor_data", sensorData);
        eventJson.set("actuator_data", actuatorData);
        eventJson.set("system_status", systemStatus);
    }
    
    // Extracción de datos adicionales específicos del evento
    
    // Sensor que activó la alarma
    if(strstr(jsonData, "\"sensor\":")) {
        ptr = strstr(jsonData, "\"sensor\":\"");
        if(ptr) {
            ptr += 10; // Saltar '"sensor":"'
            char sensor[20] = "";
            int i = 0;
            while(*ptr != '"' && i < 19) {
                sensor[i++] = *ptr++;
            }
            sensor[i] = '\0';
            eventJson.set("trigger_sensor", String(sensor));
        }
    }
    
    // Duración del evento
    if(strstr(jsonData, "\"duration\":")) {
        ptr = strstr(jsonData, "\"duration\":");
        if(ptr) {
            ptr += 11; // Saltar '"duration":'
            int duration = atoi(ptr);
            eventJson.set("duration_seconds", duration);
        }
    }
    
    // Agua utilizada
    if(strstr(jsonData, "\"water\":")) {
        ptr = strstr(jsonData, "\"water\":");
        if(ptr) {
            ptr += 8; // Saltar '"water":'
            float water = atof(ptr);
            eventJson.set("water_used", water);
        }
    }
    
    // Guardado en Firebase con nombre único
    String eventName = String(event_type) + "_" + String(millis());
    String path = "/system/history/" + category + "/" + eventName;
    
    if(Firebase.RTDB.setJSON(&fbdo, path.c_str(), &eventJson)) {
        Serial.println("[HISTORY] Event saved: " + String(event_type) + " in " + category);
    }
}
```

### Publicación de Datos de Sensores

```cpp
void publishSensorData() {
    FirebaseJson json;
    
    // Estructura de actuadores
    FirebaseJson actuators;
    actuators.set("alarm_active", alarm_active);
    actuators.set("pump_active", pump_active);
    
    // Estructura de flujo
    FirebaseJson flow;
    flow.set("current_rate", flow_rate);
    flow.set("total", total_flow);
    
    // Estructura de sensores
    FirebaseJson sensors;
    sensors.set("temperature", temperature);
    sensors.set("flame_detected", flame_detected);
    sensors.set("flame_intensity", flame_intensity);
    sensors.set("co_ppm", co_ppm);
    
    // Estructura de estado
    FirebaseJson status;
    status.set("fire_alarm", fire_alarm);
    status.set("test_mode", trigger_test);
    status.set("shutdown", shutdown_system);
    
    // Ensamblado del JSON principal
    json.set("timestamp", getFormattedTime());
    json.set("sensors", sensors);
    json.set("actuators", actuators);
    json.set("flow", flow);
    json.set("status", status);
    
    // Publicación en Firebase
    if(firebaseConnected) {
        Firebase.RTDB.setJSON(&fbdo, "/system/sensor_data", &json);
    }
}
```

### Verificación de Comandos de Firebase

```cpp
void checkFirebaseCommands() {
    if(!wifiConnected || !firebaseConnected) return;
    if(millis() - lastCommandCheck < COMMAND_CHECK_INTERVAL) return;
    
    lastCommandCheck = millis();
    
    // Verificación de comando trigger_test
    if(Firebase.RTDB.getBool(&fbdo, "/commands/trigger_test")) {
        bool current_trigger = fbdo.to<bool>();
        
        // Detección de activación de prueba
        if(current_trigger && !last_trigger_test) {
            PIC_SERIAL.write('T'); // Enviar comando al PIC
            logCommand("trigger_test", "admin");
        }
        
        // Reset automático cuando la prueba termina
        if(last_trigger_test && !trigger_test) {
            Firebase.RTDB.setBool(&fbdo, "/commands/trigger_test", false);
        }
        
        last_trigger_test = current_trigger;
    }
    
    // Verificación de comando shutdown_system
    if(Firebase.RTDB.getBool(&fbdo, "/commands/shutdown_system")) {
        bool current_shutdown = fbdo.to<bool>();
        
        // Detección de cambio de estado
        if(current_shutdown != last_shutdown_system) {
            if(current_shutdown) {
                PIC_SERIAL.write('S'); // Comando de apagado
                Firebase.RTDB.setString(&fbdo, "/system/status", "standby");
            } else {
                PIC_SERIAL.write('R'); // Comando de reanudación
                Firebase.RTDB.setString(&fbdo, "/system/status", "online");
            }
            logCommand(current_shutdown ? "shutdown" : "resume", "admin");
        }
        last_shutdown_system = current_shutdown;
    }
}
```

### Gestión de Tiempo y Timestamps

```cpp
String getFormattedTime() {
    time_t now;
    time(&now);
    struct tm timeinfo;
    char timestamp[20];
    
    // Conversión a tiempo local
    if(localtime_r(&now, &timeinfo)) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return String(timestamp);
    } else {
        return "time-error";
    }
}
```

**Configuración NTP:**
- **Servidor:** "pool.ntp.org"
- **Zona horaria:** UTC-6 (México)
- **Formato:** YYYY-MM-DD HH:MM:SS

### Limpieza Automática de Historial

```cpp
void cleanOldHistorySimple() {
    if(!wifiConnected || !firebaseConnected) return;
    if(millis() - lastHistoryClean < 1000) return; // Cada segundo
    
    lastHistoryClean = millis();
    
    // Categorías a limpiar
    String categories[] = {"fire", "test", "status"};
    
    for(int c = 0; c < 3; c++) {
        String categoryPath = "/system/history/" + categories[c];
        
        // Obtener todos los eventos de la categoría
        if(Firebase.RTDB.getJSON(&fbdo, categoryPath.c_str())) {
            FirebaseJson json = fbdo.to<FirebaseJson>();
            FirebaseJsonData jsonData;
            
            size_t len = json.iteratorBegin();
            String key, value = "";
            int type = 0;
            
            // Iterar sobre todos los eventos
            for(size_t i = 0; i < len; i++) {
                json.iteratorGet(i, type, key, value);
                
                // Extraer timestamp del nombre del evento
                int underscorePos = key.lastIndexOf('_');
                if(underscorePos > 0) {
                    String millisStr = key.substring(underscorePos + 1);
                    unsigned long entryMillis = millisStr.toInt();
                    unsigned long currentMillis = millis();
                    
                    // Eliminar eventos más antiguos de 5 segundos
                    if(currentMillis - entryMillis > 5000) {
                        String fullEntryPath = categoryPath + "/" + key;
                        Firebase.RTDB.deleteNode(&fbdo, fullEntryPath.c_str());
                    }
                }
            }
            json.iteratorEnd();
        }
    }
}
```

### Gestión de Reconexión

#### Reconexión WiFi

```cpp
void handleWiFiReconnection() {
    // Verificar estado de conexión
    if(WiFi.status() == WL_CONNECTED) {
        digitalWrite(LED_BUILTIN, HIGH); // LED encendido = conectado
        return;
    }
    
    digitalWrite(LED_BUILTIN, LOW); // LED apagado = desconectado
    
    // Intento de reconexión cada 30 segundos
    if(millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
        Serial.println("[DEBUG] WiFi reconnecting...");
        WiFi.reconnect();
        lastReconnectAttempt = millis();
        
        // Verificación de reconexión exitosa
        if(WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            digitalWrite(LED_BUILTIN, HIGH);
        }
    }
}
```

#### Reconexión Firebase

```cpp
void handleFirebaseReconnection() {
    // Verificar estado de Firebase
    if(Firebase.ready()) {
        firebaseConnected = true;
        return;
    }
    
    // Intento de reconexión cada 30 segundos
    if(millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
        Serial.println("[DEBUG] Firebase reconnecting...");
        setupFirebase();
        lastReconnectAttempt = millis();
    }
}
```

### Reset Automático de Flujo

```cpp
void handleEmergencyFlowReset() {
    static bool was_emergency = false;
    static unsigned long emergency_end_time = 0;
    
    // Determinar si hay emergencia activa
    bool current_emergency = fire_alarm || pump_active || alarm_active;
    
    // Si salimos de una emergencia, resetear flujo después de 2 segundos
    if(was_emergency && !current_emergency) {
        if(emergency_end_time == 0) {
            emergency_end_time = millis(); // Marcar fin de emergencia
        } else if(millis() - emergency_end_time >= 2000) {
            PIC_SERIAL.write('F'); // Enviar comando de reset al PIC
            total_flow = 0.0;      // Reset local
            emergency_end_time = 0;
            Serial.println("[DEBUG] Emergency flow reset after 2s");
        }
    } else if(current_emergency) {
        emergency_end_time = 0; // Reset timer si volvemos a emergencia
    }
    
    was_emergency = current_emergency;
}
```

### Registro de Comandos

```cpp
void logCommand(String command_type, String author) {
    if(!firebaseConnected) return;
    
    // Obtener timestamp actual
    time_t now;
    time(&now);
    struct tm timeinfo;
    char timestamp[20];
    
    if(localtime_r(&now, &timeinfo)) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
    } else {
        strcpy(timestamp, "time-error");
    }
    
    // Crear JSON del comando
    FirebaseJson json;
    json.set("type", command_type);
    json.set("author", author);
    json.set("timestamp", String(timestamp));
    
    // Guardar en Firebase
    Firebase.RTDB.setJSON(&fbdo, "/system/commands/last_command", &json);
}
```

### Función de Indicación Visual

```cpp
void blinkLED(int times, int delayMs) {
    for(int i = 0; i < times; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(delayMs);
        digitalWrite(LED_BUILTIN, LOW);
        delay(delayMs);
    }
}
```

## Protocolo UART Detallado

### Configuración de Hardware

**Parámetros UART:**
- **Velocidad:** 9600 baudios
- **Bits de datos:** 8
- **Paridad:** Ninguna
- **Bits de stop:** 1
- **Control de flujo:** Ninguno

**Cálculo de timing:**

Tiempo por bit = 1 / 9600 = 104.17 μs
Tiempo por carácter (10 bits) = 1.042 ms
Buffer de 200 caracteres = 208.4 ms máximo


### Formato de Mensajes

#### Del PIC al ESP32

**Datos de sensores (cada 250ms):**
```json
{"t":25.5,"fd":1,"fi":45.2,"co":25.0,"fr":2.5,"tf":15.8,"p":1,"a":1,"fa":1,"cmd":{"test":0,"shutdown":0}}\r\n
```

**Eventos históricos:**
```json
{"event":"fire_start","time":12345,"sensor":"flame_sensor"}\r\n
{"event":"fire_end","time":15678,"duration":33,"water":2.5}\r\n
{"event":"test_start","time":20000}\r\n
{"event":"test_end","time":23333,"water":1.2}\r\n
```

#### Del ESP32 al PIC

**Comandos de control:**
- `T` - Iniciar prueba del sistema
- `S` - Apagar sistema (standby)
- `R` - Reanudar operación normal
- `F` - Reset contador de flujo

### Gestión de Errores de Comunicación

1. **Buffer overflow:** Protección con límite de 200 caracteres
2. **Caracteres perdidos:** Validación de JSON con '{'
3. **Mensajes incompletos:** Terminación con '\n'
4. **Ruido en línea:** Filtrado de '\r'

## Optimizaciones y Consideraciones Técnicas

### Gestión de Memoria
- **Buffer UART:** 200 bytes estático
- **JSON objects:** Creación dinámica con liberación automática
- **Strings:** Uso eficiente con referencias

### Timing Crítico
- **Datos de sensores:** Cada 500ms para tiempo real
- **Comandos:** Verificación cada 2 segundos
- **Limpieza:** Cada segundo para mantener base de datos limpia
- **Reconexión:** Cada 30 segundos para evitar sobrecarga

### Robustez del Sistema
- **Reconexión automática:** WiFi y Firebase
- **Validación de datos:** JSON y tipos de datos
- **Timeouts:** Evitan bloqueos indefinidos
- **Estados previos:** Detección de cambios
- **Limpieza automática:** Previene acumulación de datos

### Eficiencia Energética
- **Dual core:** Aprovechamiento de arquitectura ESP32
- **Interrupciones:** Procesamiento eficiente
- **Sleep modes:** No implementado (sistema crítico 24/7)

Esta documentación técnica detallada proporciona una comprensión completa del funcionamiento del ESP32 como gateway IoT, incluyendo todos los protocolos de comunicación, algoritmos de procesamiento y optimizaciones implementadas.