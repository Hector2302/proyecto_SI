//
//  LocalNotificationService.swift
//  Firefighting system
//
//  Created by Hector Alvarado Mares on 22/05/25.
//

import Foundation
import UserNotifications
import UIKit

class LocalNotificationService: NSObject, ObservableObject {
    static let shared = LocalNotificationService()
    
    @Published var notificationPermissionGranted = false
    
    private override init() {
        super.init()
        requestNotificationPermission()
    }
    
    // MARK: - Permission Management
    
    func requestNotificationPermission() {
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .badge, .sound, .criticalAlert]) { [weak self] granted, error in
            DispatchQueue.main.async {
                self?.notificationPermissionGranted = granted
                if granted {
                    print("✅ Permisos de notificación local concedidos")
                } else {
                    print("❌ Permisos de notificación local denegados")
                }
                
                if let error = error {
                    print("❌ Error al solicitar permisos de notificación: \(error.localizedDescription)")
                }
            }
        }
    }
    
    // MARK: - Notification Types
    
    enum NotificationType: String, CaseIterable {
        case fireDetected = "fire_detected"
        case highTemperature = "high_temperature"
        case highCO = "high_co"
        case systemModeChange = "system_mode_change"
        case connectionLost = "connection_lost"
        
        var title: String {
            switch self {
            case .fireDetected:
                return "🔥 ALERTA DE INCENDIO"
            case .highTemperature:
                return "🌡️ TEMPERATURA ALTA"
            case .highCO:
                return "☠️ MONÓXIDO DE CARBONO ALTO"
            case .systemModeChange:
                return "⚙️ CAMBIO DE MODO DEL SISTEMA"
            case .connectionLost:
                return "📡 CONEXIÓN PERDIDA"
            }
        }
        
        var sound: UNNotificationSound {
            switch self {
            case .fireDetected, .highTemperature, .highCO:
                return UNNotificationSound.defaultCritical
            case .systemModeChange, .connectionLost:
                return UNNotificationSound.default
            }
        }
        
        var isCritical: Bool {
            switch self {
            case .fireDetected, .highTemperature, .highCO:
                return true
            case .systemModeChange, .connectionLost:
                return false
            }
        }
    }
    
    // MARK: - Send Notifications
    
    func sendNotification(type: NotificationType, message: String, userInfo: [String: Any] = [:]) {
        guard notificationPermissionGranted else {
            print("❌ No hay permisos para enviar notificaciones")
            return
        }
        
        let content = UNMutableNotificationContent()
        content.title = type.title
        content.body = message
        content.sound = type.sound
        content.userInfo = userInfo
        
        // Configurar como crítica si es necesario
        if type.isCritical {
            content.interruptionLevel = .critical
        }
        
        // Agregar badge
        content.badge = NSNumber(value: UIApplication.shared.applicationIconBadgeNumber + 1)
        
        // Crear request
        let identifier = "\(type.rawValue)_\(Date().timeIntervalSince1970)"
        let request = UNNotificationRequest(identifier: identifier, content: content, trigger: nil)
        
        // Enviar notificación
        UNUserNotificationCenter.current().add(request) { error in
            if let error = error {
                print("❌ Error al enviar notificación: \(error.localizedDescription)")
            } else {
                print("✅ Notificación enviada: \(type.title)")
            }
        }
    }
    
    // MARK: - Specific Notification Methods
    
    func sendFireAlert(temperature: Double, location: String = "Sistema") {
        let message = "Fuego detectado en \(location). Temperatura: \(String(format: "%.1f", temperature))°C"
        let userInfo: [String: Any] = [
            "type": "fire_alert",
            "temperature": temperature,
            "location": location,
            "timestamp": Date().timeIntervalSince1970
        ]
        sendNotification(type: .fireDetected, message: message, userInfo: userInfo)
    }
    
    func sendHighTemperatureAlert(temperature: Double, threshold: Double = 60.0) {
        let message = "Temperatura crítica detectada: \(String(format: "%.1f", temperature))°C (Límite: \(String(format: "%.1f", threshold))°C)"
        let userInfo: [String: Any] = [
            "type": "high_temperature",
            "temperature": temperature,
            "threshold": threshold,
            "timestamp": Date().timeIntervalSince1970
        ]
        sendNotification(type: .highTemperature, message: message, userInfo: userInfo)
    }
    
    func sendHighCOAlert(coLevel: Double, threshold: Double = 50.0) {
        let message = "Nivel peligroso de CO detectado: \(String(format: "%.1f", coLevel)) ppm (Límite: \(String(format: "%.1f", threshold)) ppm)"
        let userInfo: [String: Any] = [
            "type": "high_co",
            "co_level": coLevel,
            "threshold": threshold,
            "timestamp": Date().timeIntervalSince1970
        ]
        sendNotification(type: .highCO, message: message, userInfo: userInfo)
    }
    
    func sendSystemModeChangeAlert(newMode: String, previousMode: String) {
        let message = "El sistema cambió de modo \(previousMode) a \(newMode)"
        let userInfo: [String: Any] = [
            "type": "mode_change",
            "new_mode": newMode,
            "previous_mode": previousMode,
            "timestamp": Date().timeIntervalSince1970
        ]
        sendNotification(type: .systemModeChange, message: message, userInfo: userInfo)
    }
    
    func sendConnectionLostAlert() {
        let message = "Se perdió la conexión con el sistema de extinción. Verificando reconexión..."
        let userInfo: [String: Any] = [
            "type": "connection_lost",
            "timestamp": Date().timeIntervalSince1970
        ]
        sendNotification(type: .connectionLost, message: message, userInfo: userInfo)
    }
    
    // MARK: - Utility Methods
    
    func clearAllNotifications() {
        UNUserNotificationCenter.current().removeAllPendingNotificationRequests()
        UNUserNotificationCenter.current().removeAllDeliveredNotifications()
        UIApplication.shared.applicationIconBadgeNumber = 0
        print("🧹 Todas las notificaciones han sido limpiadas")
    }
    
    func clearBadge() {
        UIApplication.shared.applicationIconBadgeNumber = 0
    }
    
    func checkNotificationSettings() {
        UNUserNotificationCenter.current().getNotificationSettings { settings in
            DispatchQueue.main.async {
                self.notificationPermissionGranted = settings.authorizationStatus == .authorized
                print("📱 Estado de notificaciones: \(settings.authorizationStatus.rawValue)")
            }
        }
    }
}