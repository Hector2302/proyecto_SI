//
//  Models.swift
//  Firefighting system
//
//  Created by Hector Alvarado Mares on 22/05/25.
//

import Foundation

// MARK: - Sistema Principal
struct FirefightingSystem: Codable {
    let system: SystemData
    let commands: Commands?
    
    enum CodingKeys: String, CodingKey {
        case system
        case commands
    }
    
    // Computed property para mantener compatibilidad con el código existente
    var sensor_data: SensorData {
        return system.sensor_data
    }
}

// MARK: - Sistema de datos
struct SystemData: Codable {
    let sensor_data: SensorData
    let status: String  // Cambiar a String para coincidir con Firebase
    let commands: Commands?
    
    enum CodingKeys: String, CodingKey {
        case sensor_data = "sensor_data"
        case status
        case commands
    }
}

// MARK: - Datos de Sensores
struct SensorData: Codable {
    let sensors: Sensors
    let actuators: Actuators
    let flow: Flow
    let status: SensorStatus
    let timestamp: String
    
    enum CodingKeys: String, CodingKey {
        case sensors
        case actuators
        case flow
        case status
        case timestamp
    }
}

// MARK: - Estado de Sensores (mantener como está)
struct SensorStatus: Codable {
    let fire_alarm: Bool
    let shutdown: Bool
    let test_mode: Bool
    
    enum CodingKeys: String, CodingKey {
        case fire_alarm = "fire_alarm"
        case shutdown
        case test_mode = "test_mode"
    }
}

// MARK: - Estado del Sistema
struct SystemStatus: Codable {
    let fire_alarm: Bool
    let test_mode: Bool
    let shutdown: Bool
    
    enum CodingKeys: String, CodingKey {
        case fire_alarm = "fire_alarm"
        case test_mode = "test_mode"
        case shutdown
    }
}

// MARK: - Sensores
struct Sensors: Codable {
    let temperature: Double
    let co_ppm: Double
    let flame_detected: Bool
    let flame_intensity: Double
    
    enum CodingKeys: String, CodingKey {
        case temperature
        case co_ppm = "co_ppm"
        case flame_detected = "flame_detected"
        case flame_intensity = "flame_intensity"
    }
}

// MARK: - Flujo de Agua
struct Flow: Codable {
    let current_rate: Double
    let total: Double
    
    enum CodingKeys: String, CodingKey {
        case current_rate = "current_rate"
        case total
    }
}

// MARK: - Logs
struct Logs: Codable {
    let events: [LogEvent]
}

struct LogEvent: Codable {
    let type: String
    let timestamp: String
    let details: String
}

// MARK: - Config
struct Config: Codable {
    let users: [String: UserConfig]
}

struct UserConfig: Codable {
    let email: String
    let role: String
}

// MARK: - Actuadores
struct Actuators: Codable {
    let alarm_active: Bool
    let pump_active: Bool
    
    enum CodingKeys: String, CodingKey {
        case alarm_active = "alarm_active"
        case pump_active = "pump_active"
    }
}

// MARK: - Sistema
struct System: Codable {
    let status: String
    
    enum CodingKeys: String, CodingKey {
        case status
    }
}



// MARK: - Comandos
struct Commands: Codable {
    let trigger_test: Bool?
    let shutdown_system: Bool?
    let last_command: LastCommand?
    
    enum CodingKeys: String, CodingKey {
        case trigger_test = "trigger_test"
        case shutdown_system = "shutdown_system"
        case last_command = "last_command"
    }
}

struct LastCommand: Codable {
    let type: String
    let author: String
    let timestamp: String
}

// MARK: - Notificaciones
struct Notifications: Codable {
    let active: Bool?
    let lastUpdated: String?
    let queue: [String: NotificationItem]?
    
    enum CodingKeys: String, CodingKey {
        case active
        case lastUpdated = "last_updated"
        case queue
    }
}

struct NotificationItem: Codable {
    let type: String
    let title: String
    let message: String
    let priority: String
    let timestamp: String
    let read: Bool
}

// MARK: - Historial
struct HistoryEvent: Codable {
    let type: String?
    let timestamp: Int?
    let start_co: Double?
    let start_fi: Double?
    let start_temp: Double?
    let start_time: Int?
    let water_used: Int?
    
    enum CodingKeys: String, CodingKey {
        case type
        case timestamp
        case start_co = "start_co"
        case start_fi = "start_fi"
        case start_temp = "start_temp"
        case start_time = "start_time"
        case water_used = "water_used"
    }
}

// MARK: - Helper para decodificar valores dinámicos
struct AnyCodable: Codable {
    let value: Any
    
    init<T>(_ value: T?) {
        self.value = value ?? ()
    }
    
    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        
        if let intValue = try? container.decode(Int.self) {
            value = intValue
        } else if let doubleValue = try? container.decode(Double.self) {
            value = doubleValue
        } else if let stringValue = try? container.decode(String.self) {
            value = stringValue
        } else if let boolValue = try? container.decode(Bool.self) {
            value = boolValue
        } else if let arrayValue = try? container.decode([AnyCodable].self) {
            value = arrayValue.map { $0.value }
        } else if let dictValue = try? container.decode([String: AnyCodable].self) {
            value = dictValue.mapValues { $0.value }
        } else {
            value = ()
        }
    }
    
    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        
        switch value {
        case let intValue as Int:
            try container.encode(intValue)
        case let doubleValue as Double:
            try container.encode(doubleValue)
        case let stringValue as String:
            try container.encode(stringValue)
        case let boolValue as Bool:
            try container.encode(boolValue)
        case let arrayValue as [Any]:
            try container.encode(arrayValue.map { AnyCodable($0) })
        case let dictValue as [String: Any]:
            try container.encode(dictValue.mapValues { AnyCodable($0) })
        default:
            try container.encodeNil()
        }
    }
}

// MARK: - Extensiones para facilitar el uso
extension FirefightingSystem {
    var isEmergency: Bool {
        return sensor_data.status.fire_alarm || sensor_data.sensors.flame_detected
    }
    
    var isWarning: Bool {
        return sensor_data.sensors.co_ppm > 100
    }
    
    var isSystemActive: Bool {
        // Usar sensor_data.status en lugar de system.status
        return sensor_data.status.fire_alarm == false && sensor_data.status.shutdown == false
    }
}

extension Sensors {
    var isTemperatureHigh: Bool {
        return temperature > 50.0
    }
    
    var isTemperatureCritical: Bool {
        return temperature > 80.0
    }
    
    var isCODangerous: Bool {
        return co_ppm > 100
    }
    
    var isCritical: Bool {
        return co_ppm > 500
    }
}

// MARK: - Modelos para datos históricos de Firestore

// Promedios horarios de sensores
struct SensorHourlyAverage: Codable, Identifiable {
    let id: String
    let timestamp: String
    let temperature: TemperatureAverage
    let coLevel: COLevelAverage
    let flame: FlameAverage
    let sampleCount: Int
    
    enum CodingKeys: String, CodingKey {
        case id
        case timestamp
        case temperature
        case coLevel = "co_level"
        case flame
        case sampleCount = "sample_count"
    }
}

struct TemperatureAverage: Codable {
    let avg: Double
    let min: Double
    let max: Double
}

struct COLevelAverage: Codable {
    let avgPpm: Int
    let maxPpm: Int
    
    enum CodingKeys: String, CodingKey {
        case avgPpm = "avg_ppm"
        case maxPpm = "max_ppm"
    }
}

struct FlameAverage: Codable {
    let detectedCount: Int
    let avgIntensity: Double
    
    enum CodingKeys: String, CodingKey {
        case detectedCount = "detected_count"
        case avgIntensity = "avg_intensity"
    }
}

// Eventos de incendio y pruebas del sistema
struct FireAndTestEvent: Codable, Identifiable {
    let id: String
    let eventType: String
    let timestamp: String
    let source: String
    let data: FireEventData?
    
    enum CodingKeys: String, CodingKey {
        case id
        case eventType = "event_type"
        case timestamp
        case source
        case data
    }
}

struct FireEventData: Codable {
    let flameIntensity: Double?
    let temperatureAtEvent: Double?
    let coLevelAtEvent: Int?
    let alarmDuration: Int?
    let pumpRuntime: Int?
    
    enum CodingKeys: String, CodingKey {
        case flameIntensity = "flame_intensity"
        case temperatureAtEvent = "temperature_at_event"
        case coLevelAtEvent = "co_level_at_event"
        case alarmDuration = "alarm_duration"
        case pumpRuntime = "pump_runtime"
    }
}

// Errores de sensores
struct SensorError: Codable, Identifiable {
    let id: String
    let sensor: String
    let status: String
    let timestamp: String
    let lastOkTimestamp: String
    let resolved: Bool
    
    enum CodingKeys: String, CodingKey {
        case id
        case sensor
        case status
        case timestamp
        case lastOkTimestamp = "last_ok_timestamp"
        case resolved
    }
}

// Eventos de señal débil
struct WeakSignalEvent: Codable, Identifiable {
    let id: String
    let eventType: String
    let timestamp: String
    let signalStrengthDbm: Int
    let durationSeconds: Int
    let source: String
    
    enum CodingKeys: String, CodingKey {
        case id
        case eventType = "event_type"
        case timestamp
        case signalStrengthDbm = "signal_strength_dbm"
        case durationSeconds = "duration_seconds"
        case source
    }
}

// Eventos de modo del sistema (shutdown/startup)
struct SystemModeEvent: Codable, Identifiable {
    let id: String
    let eventType: String
    let timestamp: String
    let initiatedBy: String
    let source: String
    let previousMode: String?
    let alarmActive: Bool?
    let pumpActive: Bool?
    let downtimeSeconds: Int?
    let newMode: String?
    
    enum CodingKeys: String, CodingKey {
        case id
        case eventType = "event_type"
        case timestamp
        case initiatedBy = "initiated_by"
        case source
        case previousMode = "previous_mode"
        case alarmActive = "alarm_active"
        case pumpActive = "pump_active"
        case downtimeSeconds = "downtime_seconds"
        case newMode = "new_mode"
    }
}

// Notificaciones del sistema
struct SystemNotification: Codable, Identifiable {
    let id: String
    let notificationId: String
    let type: String
    let title: String
    let message: String
    let priority: String
    let timestamp: String
    let read: Bool
    let source: String
    let relatedData: [String: AnyCodable]
    
    enum CodingKeys: String, CodingKey {
        case id
        case notificationId = "notification_id"
        case type
        case title
        case message
        case priority
        case timestamp
        case read
        case source
        case relatedData = "related_data"
    }
}

// Modelo para respuesta de Firestore con metadatos
struct FirestoreResponse<T: Codable>: Codable {
    let documents: [T]
    let totalCount: Int
    let lastDocument: String?
    
    enum CodingKeys: String, CodingKey {
        case documents
        case totalCount = "total_count"
        case lastDocument = "last_document"
    }
}


// MARK: - Modelos para eventos específicos del sistema

struct FireEvent: Codable, Identifiable {
    let id: String
    let eventType: String
    let category: String
    let timestamp: String
    let source: String
    let eventKey: String
    let triggerSensor: String?
    let durationSeconds: Int?
    let waterUsed: Double?
    let sensorData: SensorData?
    let actuatorData: Actuators?
    let systemStatus: String?
    
    enum CodingKeys: String, CodingKey {
        case id
        case eventType = "event_type"
        case category
        case timestamp
        case source
        case eventKey = "event_key"
        case triggerSensor = "trigger_sensor"
        case durationSeconds = "duration_seconds"
        case waterUsed = "water_used"
        case sensorData = "sensor_data"
        case actuatorData = "actuator_data"
        case systemStatus = "system_status"
    }
    
    var formattedDate: String {
        let formatter = ISO8601DateFormatter()
        guard let date = formatter.date(from: timestamp) else { return timestamp }
        
        let displayFormatter = DateFormatter()
        displayFormatter.dateStyle = .medium
        displayFormatter.timeStyle = .short
        return displayFormatter.string(from: date)
    }
    
    var eventDescription: String {
        switch eventType {
        case "fire_start":
            return "🔥 Incendio Detectado"
        case "fire_end":
            return "✅ Incendio Extinguido"
        default:
            return "🔥 Evento de Incendio"
        }
    }
}

struct TestEvent: Codable, Identifiable {
    let id: String
    let eventType: String
    let category: String
    let timestamp: String
    let source: String
    let eventKey: String
    let durationSeconds: Int?
    let sensorData: SensorData?
    let actuatorData: Actuators?
    let systemStatus: String?
    
    enum CodingKeys: String, CodingKey {
        case id
        case eventType = "event_type"
        case category
        case timestamp
        case source
        case eventKey = "event_key"
        case durationSeconds = "duration_seconds"
        case sensorData = "sensor_data"
        case actuatorData = "actuator_data"
        case systemStatus = "system_status"
    }
    
    var formattedDate: String {
        let formatter = ISO8601DateFormatter()
        guard let date = formatter.date(from: timestamp) else { return timestamp }
        
        let displayFormatter = DateFormatter()
        displayFormatter.dateStyle = .medium
        displayFormatter.timeStyle = .short
        return displayFormatter.string(from: date)
    }
    
    var eventDescription: String {
        switch eventType {
        case "test_start":
            return "🔧 Prueba Iniciada"
        case "test_end":
            return "✅ Prueba Completada"
        default:
            return "🔧 Evento de Prueba"
        }
    }
}

struct SystemStatusEvent: Codable, Identifiable {
    let id: String
    let eventType: String
    let category: String
    let timestamp: String
    let source: String
    let eventKey: String
    let durationSeconds: Int?
    
    enum CodingKeys: String, CodingKey {
        case id
        case eventType = "event_type"
        case category
        case timestamp
        case source
        case eventKey = "event_key"
        case durationSeconds = "duration_seconds"
    }
    
    var formattedDate: String {
        let formatter = ISO8601DateFormatter()
        guard let date = formatter.date(from: timestamp) else { return timestamp }
        
        let displayFormatter = DateFormatter()
        displayFormatter.dateStyle = .medium
        displayFormatter.timeStyle = .short
        return displayFormatter.string(from: date)
    }
    
    var eventDescription: String {
        switch eventType {
        case "shutdown_start":
            return "⏹️ Sistema Apagado"
        case "shutdown_end":
            return "▶️ Sistema Reiniciado"
        default:
            return "⚙️ Comando del Sistema"
        }
    }
}
