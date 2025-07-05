//
//  FirebaseService.swift
//  Firefighting system
//
//  Created by Hector Alvarado Mares on 22/05/25.
//

import Foundation
import FirebaseAuth
import FirebaseDatabase
import FirebaseMessaging
import Combine
import UserNotifications
import UIKit

class FirebaseService: NSObject, ObservableObject {
    static let shared = FirebaseService()
    
    @Published var isAuthenticated = false
    @Published var currentUser: User?
    @Published var systemData: FirefightingSystem?
    @Published var isLoading = false
    @Published var errorMessage: String?
    @Published var connectionStatus: ConnectionStatus = .disconnected
    @Published var lastDataUpdate: Date?
    @Published var fcmToken: String?
    @Published var notificationPermissionGranted = false
    
    // Local Notifications
    private let localNotificationService = LocalNotificationService.shared
    private var lastNotificationTime: [String: Date] = [:]
    private let notificationCooldown: TimeInterval = 30 // 30 segundos entre notificaciones del mismo tipo
    
    private var cancellables = Set<AnyCancellable>()
    private var databaseRef: DatabaseReference
    private var systemRef: DatabaseReference
    private var authStateListener: AuthStateDidChangeListenerHandle?
    private var systemDataListener: DatabaseHandle?
    private var connectionTimer: Timer?
    private var connectedRef: DatabaseReference?
    
    enum ConnectionStatus {
        case connected
        case disconnected
        case noData
        case error
    }
    
    private override init() {
        self.databaseRef = Database.database().reference()
        self.systemRef = databaseRef.child("system")
        
        super.init()
        
        setupAuthStateListener()
        setupConnectionMonitoring()
    }
    
    deinit {
        removeListeners()
        connectionTimer?.invalidate()
    }
    
    // MARK: - Autenticación
    
    private func setupAuthStateListener() {
        authStateListener = Auth.auth().addStateDidChangeListener { [weak self] _, user in
            DispatchQueue.main.async {
                self?.currentUser = user
                self?.isAuthenticated = user != nil
                
                if user != nil {
                    self?.startListeningToSystemData()
                    self?.setupNotifications()
                } else {
                    self?.stopListeningToSystemData()
                }
            }
        }
    }
    
    func signIn(email: String, password: String) {
        // Validaciones básicas
        guard !email.isEmpty else {
            DispatchQueue.main.async {
                self.errorMessage = "Por favor ingresa tu correo electrónico"
            }
            return
        }
        
        guard !password.isEmpty else {
            DispatchQueue.main.async {
                self.errorMessage = "Por favor ingresa tu contraseña"
            }
            return
        }
        
        isLoading = true
        errorMessage = nil
        connectionStatus = .disconnected
        
        Auth.auth().signIn(withEmail: email, password: password) { [weak self] result, error in
            DispatchQueue.main.async {
                self?.isLoading = false
                
                if let error = error {
                    self?.connectionStatus = .error
                    self?.errorMessage = self?.getLocalizedErrorMessage(error) ?? "Error desconocido"
                    print("❌ Error de autenticación: \(error.localizedDescription)")
                } else {
                    self?.errorMessage = nil
                    print("✅ Usuario autenticado exitosamente")
                    // La conexión a la base de datos se iniciará automáticamente en setupAuthStateListener
                }
            }
        }
    }
    
    // Alias para compatibilidad
    func login(email: String, password: String) {
        signIn(email: email, password: password)
    }
    
    func signOut() {
        do {
            try Auth.auth().signOut()
            DispatchQueue.main.async {
                self.systemData = nil
                self.errorMessage = nil
                self.connectionStatus = .disconnected
                self.lastDataUpdate = nil
                print("✅ Sesión cerrada exitosamente")
            }
        } catch let error {
            DispatchQueue.main.async {
                self.errorMessage = "Error al cerrar sesión: \(error.localizedDescription)"
                print("❌ Error al cerrar sesión: \(error.localizedDescription)")
            }
        }
    }
    
    // Alias para compatibilidad
    func logout() {
        signOut()
    }
    
    private func getLocalizedErrorMessage(_ error: Error) -> String {
        if let authError = error as NSError? {
            switch authError.code {
            case AuthErrorCode.invalidEmail.rawValue:
                return "Correo electrónico inválido"
            case AuthErrorCode.wrongPassword.rawValue:
                return "Contraseña incorrecta"
            case AuthErrorCode.userNotFound.rawValue:
                return "Usuario no encontrado"
            case AuthErrorCode.userDisabled.rawValue:
                return "Cuenta deshabilitada"
            case AuthErrorCode.tooManyRequests.rawValue:
                return "Demasiados intentos. Intenta más tarde"
            case AuthErrorCode.networkError.rawValue:
                return "Error de conexión"
            default:
                return "Error de autenticación: \(error.localizedDescription)"
            }
        }
        return error.localizedDescription
    }
    
    // MARK: - Base de Datos en Tiempo Real
    
    private func startListeningToSystemData() {
        print("🔄 Iniciando escucha de datos del sistema...")
        
        // Primero verificar conectividad
        connectedRef = Database.database().reference().child(".info/connected")
        connectedRef?.observe(.value) { [weak self] (snapshot: DataSnapshot) in
            guard let self = self else { return }
            
            if let connected = snapshot.value as? Bool, connected {
                DispatchQueue.main.async {
                    print("🌐 Conectado a Firebase")
                    if self.connectionStatus == .disconnected {
                        self.connectionStatus = .connected
                        self.errorMessage = nil
                    }
                }
            } else {
                DispatchQueue.main.async {
                    print("❌ Desconectado de Firebase")
                    self.connectionStatus = .disconnected
                    self.errorMessage = "Sin conexión a Firebase. Verifica tu conexión a internet."
                }
            }
        }
        
        // Escuchar datos del sistema
        print("📡 Configurando listener para la ruta raíz")
        // Ahora escuchamos la ruta raíz en lugar de /system
        systemDataListener = databaseRef.observe(.value, with: { [weak self] (snapshot: DataSnapshot) in
            guard let self = self else { return }
            
            print("📊 Snapshot recibido - Existe: \(snapshot.exists()), Valor: \(snapshot.value != nil ? "Sí" : "No")")
            
            DispatchQueue.main.async {
                if snapshot.exists() {
                    do {
                        let previousData = self.systemData
                        if let data = snapshot.value as? [String: Any] {
                            print("🔍 Datos encontrados en Firebase: \(data.keys.joined(separator: ", "))")
                            let jsonData = try JSONSerialization.data(withJSONObject: data)
                            let systemData = try JSONDecoder().decode(FirefightingSystem.self, from: jsonData)
                            
                            self.systemData = systemData
                            self.lastDataUpdate = Date()
                            self.connectionStatus = .connected
                            self.errorMessage = nil
                            
                            // Detectar cambios críticos y enviar notificaciones
                            self.detectCriticalChanges(previousData: previousData, newData: systemData)
                            
                            print("✅ Datos procesados exitosamente:")
                            print("   📊 Temperatura: \(systemData.sensor_data.sensors.temperature)°C")
                            print("   🔥 CO: \(systemData.sensor_data.sensors.co_ppm) ppm")
                            print("   💧 Flujo de agua: \(systemData.sensor_data.flow.current_rate) L/min")
                            print("   🔥 Llama detectada: \(systemData.sensor_data.sensors.flame_detected ? "Sí" : "No")")
                        } else {
                            print("⚠️ Datos recibidos pero no son del tipo esperado")
                            self.connectionStatus = .noData
                            self.errorMessage = "Formato de datos incorrecto en Firebase"
                        }
                    } catch {
                        print("❌ Error al decodificar datos: \(error)")
                        self.connectionStatus = .error
                        self.errorMessage = "Error al procesar datos del simulador: \(error.localizedDescription)"
                    }
                } else {
                    print("⚠️ No existen datos en la ruta /system de Firebase")
                    self.connectionStatus = .noData
                    self.errorMessage = "El simulador no ha enviado datos aún. Verifica que esté ejecutándose."
                }
            }
        }, withCancel: { [weak self] error in
            print("❌ Error en el listener de Firebase: \(error)")
            DispatchQueue.main.async {
                self?.connectionStatus = .error
                self?.errorMessage = "Error de conexión con la base de datos: \(error.localizedDescription)"
            }
        })
    }
    
    private func stopListeningToSystemData() {
        if let systemDataListener = systemDataListener {
            systemRef.removeObserver(withHandle: systemDataListener)
            self.systemDataListener = nil
        }
        
        if let connectedRef = connectedRef {
            connectedRef.removeAllObservers()
            self.connectedRef = nil
        }
        
        DispatchQueue.main.async {
            self.connectionStatus = .disconnected
            self.systemData = nil
            self.errorMessage = nil
        }
    }
    
    // MARK: - Comandos del Sistema
    
    func triggerSystemTest() {
        guard isAuthenticated else { return }
        
        let commandsRef = databaseRef.child("commands")
        commandsRef.updateChildValues([
            "trigger_test": true,
            "last_command": [
                "type": "manual_test",
                "author": "mobile_app",
                "timestamp": ISO8601DateFormatter().string(from: Date())
            ]
        ]) { [weak self] error, _ in
            if let error = error {
                DispatchQueue.main.async {
                    self?.errorMessage = "Error al ejecutar prueba: \(error.localizedDescription)"
                }
            }
        }
    }
    
    func toggleSystemShutdown() {
        guard isAuthenticated, let currentData = systemData else { return }
        
        let newShutdownState = !(currentData.system?.status == "online")
        
        let commandsRef = databaseRef.child("commands")
        commandsRef.updateChildValues([
            "shutdown_system": newShutdownState,
            "last_command": [
                "type": newShutdownState ? "system_shutdown" : "system_startup",
                "author": "mobile_app",
                "timestamp": ISO8601DateFormatter().string(from: Date())
            ]
        ]) { [weak self] error, _ in
            if let error = error {
                DispatchQueue.main.async {
                    self?.errorMessage = "Error al cambiar estado del sistema: \(error.localizedDescription)"
                }
            }
        }
    }
    
    // MARK: - Limpieza
    
    // MARK: - Monitoreo de Conexión
    
    private func setupConnectionMonitoring() {
        // Timer para verificar si los datos están siendo actualizados
        connectionTimer = Timer.scheduledTimer(withTimeInterval: 10.0, repeats: true) { [weak self] _ in
            self?.checkDataFreshness()
        }
    }
    
    private func checkDataFreshness() {
        guard let lastUpdate = lastDataUpdate else {
            DispatchQueue.main.async {
                if self.connectionStatus == .connected {
                    self.connectionStatus = .noData
                    self.errorMessage = "No se han recibido datos del simulador"
                }
            }
            return
        }
        
        let timeSinceLastUpdate = Date().timeIntervalSince(lastUpdate)
        
        // Si no hay datos nuevos en más de 15 segundos, considerar pérdida de conexión
        if timeSinceLastUpdate > 15.0 {
            DispatchQueue.main.async {
                self.connectionStatus = .noData
                self.errorMessage = "Pérdida de conexión con el simulador. Última actualización: \(Int(timeSinceLastUpdate))s atrás"
            }
        }
    }
    
    // MARK: - Limpieza
    
    private func removeListeners() {
        if let authListener = authStateListener {
            Auth.auth().removeStateDidChangeListener(authListener)
        }
        stopListeningToSystemData()
        if let connectedRef = connectedRef {
            connectedRef.removeAllObservers()
        }
    }
}

// MARK: - Extensiones para formateo de datos

extension FirebaseService {
    func getFormattedTemperature() -> String {
        guard connectionStatus == .connected,
              let temp = systemData?.sensor_data.sensors.temperature else { 
            return "--°C" 
        }
        return String(format: "%.1f°C", temp)
    }
    
    func getFormattedCOLevel() -> String {
        guard connectionStatus == .connected,
              let co = systemData?.sensor_data.sensors.co_ppm else { 
            return "-- ppm" 
        }
        return String(format: "%.1f ppm", co)
    }
    
    func getFormattedWaterFlow() -> String {
        guard connectionStatus == .connected,
              let water = systemData?.sensor_data.flow.current_rate else { 
            return "-- L/min" 
        }
        return String(format: "%.1f L/min", water)
    }
    
    func getSystemModeText() -> String {
        guard connectionStatus == .connected,
              let status = systemData?.system?.status else { 
            return getConnectionStatusText()
        }
        
        switch status {
        case "online":
            if systemData?.sensor_data.status.fire_alarm == true {
                return "Emergencia"
            } else if systemData?.sensor_data.sensors.co_ppm ?? 0 > 100 {
                return "Advertencia"
            } else {
                return "Normal"
            }
        case "offline":
            return "En reposo"
        default:
            return status.capitalized
        }
    }
    
    func getSystemModeColor() -> String {
        guard connectionStatus == .connected,
              let status = systemData?.system?.status else { 
            return getConnectionStatusColor()
        }
        
        switch status {
        case "online":
            if systemData?.sensor_data.status.fire_alarm == true {
                return "red"
            } else if systemData?.sensor_data.sensors.co_ppm ?? 0 > 100 {
                return "orange"
            } else {
                return "green"
            }
        case "offline":
            return "blue"
        default:
            return "gray"
        }
    }
    
    private func getConnectionStatusText() -> String {
        switch connectionStatus {
        case .connected:
            return "Conectado"
        case .disconnected:
            return "Desconectado"
        case .noData:
            return "Sin Datos"
        case .error:
            return "Error"
        }
    }
    
    private func getConnectionStatusColor() -> String {
        switch connectionStatus {
        case .connected:
            return "green"
        case .disconnected, .noData, .error:
            return "red"
        }
    }
    
    // MARK: - Push Notifications
    
    func setupNotifications() {
        requestNotificationPermission()
        configureFCM()
    }
    
    private func requestNotificationPermission() {
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .badge, .sound]) { [weak self] granted, error in
            DispatchQueue.main.async {
                self?.notificationPermissionGranted = granted
                if granted {
                    print("✅ Permisos de notificación concedidos")
                    DispatchQueue.main.async {
                        UIApplication.shared.registerForRemoteNotifications()
                    }
                } else {
                    print("❌ Permisos de notificación denegados")
                }
                
                if let error = error {
                    print("❌ Error al solicitar permisos: \(error.localizedDescription)")
                }
            }
        }
    }
    
    private func configureFCM() {
        Messaging.messaging().delegate = self
        
        // Obtener token FCM
        Messaging.messaging().token { [weak self] token, error in
            if let error = error {
                print("❌ Error al obtener token FCM: \(error.localizedDescription)")
            } else if let token = token {
                DispatchQueue.main.async {
                    self?.fcmToken = token
                    print("🔑 Token FCM obtenido: \(token)")
                    print("📱 Copia este token para configurar notificaciones push:")
                    print("\(token)")
                }
            }
        }
    }
    
    func refreshFCMToken() {
        Messaging.messaging().deleteToken { [weak self] error in
            if let error = error {
                print("❌ Error al eliminar token: \(error.localizedDescription)")
            } else {
                self?.configureFCM()
            }
        }
    }
    
    // MARK: - Local Notifications
    
    private func detectCriticalChanges(previousData: FirefightingSystem?, newData: FirefightingSystem) {
        guard let previous = previousData else {
            // Primera carga de datos, verificar si hay condiciones críticas actuales
            checkCurrentCriticalConditions(data: newData)
            return
        }
        
        // Detectar fuego
        if !previous.sensor_data.sensors.flame_detected && newData.sensor_data.sensors.flame_detected {
            sendNotificationWithCooldown(type: "fire_detected") {
                self.localNotificationService.sendFireAlert(
                    temperature: newData.sensor_data.sensors.temperature,
                    location: "Sistema Principal"
                )
            }
        }
        
        // Detectar temperatura alta (umbral: 60°C)
        let tempThreshold: Double = 60.0
        if previous.sensor_data.sensors.temperature < tempThreshold && newData.sensor_data.sensors.temperature >= tempThreshold {
            sendNotificationWithCooldown(type: "high_temperature") {
                self.localNotificationService.sendHighTemperatureAlert(
                    temperature: newData.sensor_data.sensors.temperature,
                    threshold: tempThreshold
                )
            }
        }
        
        // Detectar CO alto (umbral: 50 ppm)
        let coThreshold: Double = 50.0
        if previous.sensor_data.sensors.co_ppm < coThreshold && newData.sensor_data.sensors.co_ppm >= coThreshold {
            sendNotificationWithCooldown(type: "high_co") {
                self.localNotificationService.sendHighCOAlert(
                    coLevel: newData.sensor_data.sensors.co_ppm,
                    threshold: coThreshold
                )
            }
        }
        
        // Detectar cambio de estado de alarma
        if previous.sensor_data.status.fire_alarm != newData.sensor_data.status.fire_alarm {
            sendNotificationWithCooldown(type: "mode_change") {
                self.localNotificationService.sendSystemModeChangeAlert(
                    newMode: newData.sensor_data.status.fire_alarm ? "Emergencia" : "Normal",
                    previousMode: previous.sensor_data.status.fire_alarm ? "Emergencia" : "Normal"
                )
            }
        }
        
        // Detectar pérdida de conexión (esto se maneja en otro lugar, pero podemos agregarlo aquí también)
        if connectionStatus == .disconnected || connectionStatus == .error {
            sendNotificationWithCooldown(type: "connection_lost") {
                self.localNotificationService.sendConnectionLostAlert()
            }
        }
    }
    
    private func checkCurrentCriticalConditions(data: FirefightingSystem) {
        // Verificar condiciones críticas en la primera carga
        if data.sensor_data.sensors.flame_detected {
            sendNotificationWithCooldown(type: "fire_detected") {
                self.localNotificationService.sendFireAlert(
                    temperature: data.sensor_data.sensors.temperature,
                    location: "Sistema Principal"
                )
            }
        }
        
        if data.sensor_data.sensors.temperature >= 60.0 {
            sendNotificationWithCooldown(type: "high_temperature") {
                self.localNotificationService.sendHighTemperatureAlert(
                    temperature: data.sensor_data.sensors.temperature
                )
            }
        }
        
        if data.sensor_data.sensors.co_ppm >= 50.0 {
            sendNotificationWithCooldown(type: "high_co") {
                self.localNotificationService.sendHighCOAlert(
                    coLevel: data.sensor_data.sensors.co_ppm
                )
            }
        }
    }
    
    private func sendNotificationWithCooldown(type: String, notification: @escaping () -> Void) {
        let now = Date()
        
        // Verificar si ha pasado suficiente tiempo desde la última notificación de este tipo
        if let lastTime = lastNotificationTime[type] {
            let timeSinceLastNotification = now.timeIntervalSince(lastTime)
            if timeSinceLastNotification < notificationCooldown {
                print("🔕 Notificación de tipo \(type) en cooldown. Faltan \(Int(notificationCooldown - timeSinceLastNotification)) segundos")
                return
            }
        }
        
        // Enviar notificación y actualizar timestamp
        notification()
        lastNotificationTime[type] = now
        print("📱 Notificación enviada: \(type)")
    }
    
    func clearNotificationCooldowns() {
        lastNotificationTime.removeAll()
        print("🧹 Cooldowns de notificaciones limpiados")
    }
}


// MARK: - MessagingDelegate
extension FirebaseService: MessagingDelegate {
    func messaging(_ messaging: Messaging, didReceiveRegistrationToken fcmToken: String?) {
        guard let fcmToken = fcmToken else { return }
        
        DispatchQueue.main.async {
            self.fcmToken = fcmToken
            print("🔄 Token FCM actualizado: \(fcmToken)")
            print("📱 Nuevo token para notificaciones push:")
            print("\(fcmToken)")
        }
    }
}
