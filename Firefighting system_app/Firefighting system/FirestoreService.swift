//
//  FirestoreService.swift
//  Firefighting system
//
//  Created by Hector Alvarado Mares on 26/05/25.
//

import Foundation
import FirebaseFirestore
import Combine

class FirestoreService: ObservableObject {
    static let shared = FirestoreService()
    private let db = Firestore.firestore()
    
    @Published var sensorAverages: [SensorHourlyAverage] = []
    @Published var fireAndTestEvents: [FireAndTestEvent] = []
    @Published var sensorErrors: [SensorError] = []
    @Published var weakSignalEvents: [WeakSignalEvent] = []
    @Published var systemModeEvents: [SystemModeEvent] = []
    @Published var systemNotifications: [SystemNotification] = []
    @Published var isLoading = false
    @Published var errorMessage: String?
    
    private init() {}
    
    // MARK: - Promedios horarios de sensores
    func fetchSensorAverages(limit: Int = 24, startDate: Date? = nil, endDate: Date? = nil) {
        isLoading = true
        errorMessage = nil
        
        var query: Query = db.collection("sensor_averages_by_hour")
            .order(by: "timestamp", descending: true)
            .limit(to: limit)
        
        if let startDate = startDate {
            let formatter = ISO8601DateFormatter()
            query = query.whereField("timestamp", isGreaterThanOrEqualTo: formatter.string(from: startDate))
        }
        
        if let endDate = endDate {
            let formatter = ISO8601DateFormatter()
            query = query.whereField("timestamp", isLessThanOrEqualTo: formatter.string(from: endDate))
        }
        
        query.getDocuments { [weak self] snapshot, error in
            DispatchQueue.main.async {
                self?.isLoading = false
                
                if let error = error {
                    self?.errorMessage = "Error fetching sensor averages: \(error.localizedDescription)"
                    return
                }
                
                guard let documents = snapshot?.documents else {
                    self?.sensorAverages = []
                    return
                }
                
                self?.sensorAverages = documents.compactMap { document in
                    var data = document.data()
                    data["id"] = document.documentID
                    
                    do {
                        let jsonData = try JSONSerialization.data(withJSONObject: data)
                        return try JSONDecoder().decode(SensorHourlyAverage.self, from: jsonData)
                    } catch {
                        print("Error decoding sensor average: \(error)")
                        return nil
                    }
                }
            }
        }
    }
    
    // MARK: - Eventos de incendio y pruebas
    func fetchFireAndTestEvents(limit: Int = 50) {
        isLoading = true
        errorMessage = nil
        
        db.collection("fire_and_test_events")
            .order(by: "timestamp", descending: true)
            .limit(to: limit)
            .getDocuments { [weak self] snapshot, error in
                DispatchQueue.main.async {
                    self?.isLoading = false
                    
                    if let error = error {
                        self?.errorMessage = "Error fetching fire and test events: \(error.localizedDescription)"
                        return
                    }
                    
                    guard let documents = snapshot?.documents else {
                        self?.fireAndTestEvents = []
                        return
                    }
                    
                    self?.fireAndTestEvents = documents.compactMap { document in
                        var data = document.data()
                        data["id"] = document.documentID
                        
                        do {
                            let jsonData = try JSONSerialization.data(withJSONObject: data)
                            return try JSONDecoder().decode(FireAndTestEvent.self, from: jsonData)
                        } catch {
                            print("Error decoding fire and test event: \(error)")
                            return nil
                        }
                    }
                }
            }
    }
    
    // MARK: - Errores de sensores
    func fetchSensorErrors(includeResolved: Bool = false) {
        isLoading = true
        errorMessage = nil
        
        var query: Query = db.collection("sensor_errors")
            .order(by: "timestamp", descending: true)
        
        if !includeResolved {
            query = query.whereField("resolved", isEqualTo: false)
        }
        
        query.getDocuments { [weak self] snapshot, error in
            DispatchQueue.main.async {
                self?.isLoading = false
                
                if let error = error {
                    self?.errorMessage = "Error fetching sensor errors: \(error.localizedDescription)"
                    return
                }
                
                guard let documents = snapshot?.documents else {
                    self?.sensorErrors = []
                    return
                }
                
                self?.sensorErrors = documents.compactMap { document in
                    var data = document.data()
                    data["id"] = document.documentID
                    
                    do {
                        let jsonData = try JSONSerialization.data(withJSONObject: data)
                        return try JSONDecoder().decode(SensorError.self, from: jsonData)
                    } catch {
                        print("Error decoding sensor error: \(error)")
                        return nil
                    }
                }
            }
        }
    }
    
    // MARK: - Eventos de señal débil
    func fetchWeakSignalEvents(limit: Int = 30) {
        isLoading = true
        errorMessage = nil
        
        db.collection("weak_signal_events")
            .order(by: "timestamp", descending: true)
            .limit(to: limit)
            .getDocuments { [weak self] snapshot, error in
                DispatchQueue.main.async {
                    self?.isLoading = false
                    
                    if let error = error {
                        self?.errorMessage = "Error fetching weak signal events: \(error.localizedDescription)"
                        return
                    }
                    
                    guard let documents = snapshot?.documents else {
                        self?.weakSignalEvents = []
                        return
                    }
                    
                    self?.weakSignalEvents = documents.compactMap { document in
                        var data = document.data()
                        data["id"] = document.documentID
                        
                        do {
                            let jsonData = try JSONSerialization.data(withJSONObject: data)
                            return try JSONDecoder().decode(WeakSignalEvent.self, from: jsonData)
                        } catch {
                            print("Error decoding weak signal event: \(error)")
                            return nil
                        }
                    }
                }
            }
    }
    
    // MARK: - Eventos de modo del sistema
    func fetchSystemModeEvents(limit: Int = 20) {
        isLoading = true
        errorMessage = nil
        
        db.collection("system_mode_events")
            .order(by: "timestamp", descending: true)
            .limit(to: limit)
            .getDocuments { [weak self] snapshot, error in
                DispatchQueue.main.async {
                    self?.isLoading = false
                    
                    if let error = error {
                        self?.errorMessage = "Error fetching system mode events: \(error.localizedDescription)"
                        return
                    }
                    
                    guard let documents = snapshot?.documents else {
                        self?.systemModeEvents = []
                        return
                    }
                    
                    self?.systemModeEvents = documents.compactMap { document in
                        var data = document.data()
                        data["id"] = document.documentID
                        
                        do {
                            let jsonData = try JSONSerialization.data(withJSONObject: data)
                            return try JSONDecoder().decode(SystemModeEvent.self, from: jsonData)
                        } catch {
                            print("Error decoding system mode event: \(error)")
                            return nil
                        }
                    }
                }
            }
    }
    
    // MARK: - Notificaciones del sistema
    func fetchSystemNotifications(limit: Int = 50, unreadOnly: Bool = false) {
        isLoading = true
        errorMessage = nil
        
        var query: Query = db.collection("system_notifications")
            .order(by: "timestamp", descending: true)
            .limit(to: limit)
        
        if unreadOnly {
            query = query.whereField("read", isEqualTo: false)
        }
        
        query.getDocuments { [weak self] snapshot, error in
            DispatchQueue.main.async {
                self?.isLoading = false
                
                if let error = error {
                    self?.errorMessage = "Error fetching system notifications: \(error.localizedDescription)"
                    return
                }
                
                guard let documents = snapshot?.documents else {
                    self?.systemNotifications = []
                    return
                }
                
                self?.systemNotifications = documents.compactMap { document in
                    var data = document.data()
                    data["id"] = document.documentID
                    
                    do {
                        let jsonData = try JSONSerialization.data(withJSONObject: data)
                        return try JSONDecoder().decode(SystemNotification.self, from: jsonData)
                    } catch {
                        print("Error decoding system notification: \(error)")
                        return nil
                    }
                }
            }
        }
    }
    
    // MARK: - Marcar notificación como leída
    func markNotificationAsRead(notificationId: String) {
        db.collection("system_notifications")
            .document(notificationId)
            .updateData(["read": true]) { error in
                if let error = error {
                    print("Error marking notification as read: \(error)")
                } else {
                    // Actualizar la notificación local
                    DispatchQueue.main.async {
                        if let index = self.systemNotifications.firstIndex(where: { $0.id == notificationId }) {
                            // Crear una nueva notificación con read = true
                            let updatedNotification = SystemNotification(
                                id: self.systemNotifications[index].id,
                                notificationId: self.systemNotifications[index].notificationId,
                                type: self.systemNotifications[index].type,
                                title: self.systemNotifications[index].title,
                                message: self.systemNotifications[index].message,
                                priority: self.systemNotifications[index].priority,
                                timestamp: self.systemNotifications[index].timestamp,
                                read: true,
                                source: self.systemNotifications[index].source,
                                relatedData: self.systemNotifications[index].relatedData
                            )
                            self.systemNotifications[index] = updatedNotification
                        }
                    }
                }
            }
    }
    
    // MARK: - Resolver error de sensor
    func resolveSensorError(errorId: String) {
        db.collection("sensor_errors")
            .document(errorId)
            .updateData(["resolved": true]) { error in
                if let error = error {
                    print("Error resolving sensor error: \(error)")
                } else {
                    // Actualizar el error local
                    DispatchQueue.main.async {
                        if let index = self.sensorErrors.firstIndex(where: { $0.id == errorId }) {
                            // Crear un nuevo error con resolved = true
                            let updatedError = SensorError(
                                id: self.sensorErrors[index].id,
                                sensor: self.sensorErrors[index].sensor,
                                status: self.sensorErrors[index].status,
                                timestamp: self.sensorErrors[index].timestamp,
                                lastOkTimestamp: self.sensorErrors[index].lastOkTimestamp,
                                resolved: true
                            )
                            self.sensorErrors[index] = updatedError
                        }
                    }
                }
            }
    }
    
    // MARK: - Limpiar datos locales
    func clearAllData() {
        DispatchQueue.main.async {
            self.sensorAverages = []
            self.fireAndTestEvents = []
            self.sensorErrors = []
            self.weakSignalEvents = []
            self.systemModeEvents = []
            self.systemNotifications = []
            self.errorMessage = nil
        }
    }
    
    // MARK: - Cargar todos los datos
    func loadAllHistoricalData() {
        fetchSensorAverages()
        fetchFireAndTestEvents()
        fetchSensorErrors()
        fetchWeakSignalEvents()
        fetchSystemModeEvents()
        fetchSystemNotifications()
    }
}

// MARK: - Extensiones para formateo de fechas
extension FirestoreService {
    func formatTimestamp(_ timestamp: String) -> String {
        let formatter = ISO8601DateFormatter()
        guard let date = formatter.date(from: timestamp) else {
            return timestamp
        }
        
        let displayFormatter = DateFormatter()
        displayFormatter.dateStyle = .medium
        displayFormatter.timeStyle = .short
        displayFormatter.locale = Locale(identifier: "es_ES")
        
        return displayFormatter.string(from: date)
    }
    
    func formatRelativeTime(_ timestamp: String) -> String {
        let formatter = ISO8601DateFormatter()
        guard let date = formatter.date(from: timestamp) else {
            return timestamp
        }
        
        let now = Date()
        let timeInterval = now.timeIntervalSince(date)
        
        if timeInterval < 60 {
            return "Hace \(Int(timeInterval)) segundos"
        } else if timeInterval < 3600 {
            return "Hace \(Int(timeInterval / 60)) minutos"
        } else if timeInterval < 86400 {
            return "Hace \(Int(timeInterval / 3600)) horas"
        } else {
            return "Hace \(Int(timeInterval / 86400)) días"
        }
    }
}