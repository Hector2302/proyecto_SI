//
//  DashboardView.swift
//  Firefighting system
//
//  Created by Hector Alvarado Mares on 22/05/25.
//

import SwiftUI
import UIKit

struct DashboardView: View {
    @ObservedObject var firebaseService: FirebaseService
    @State private var showingAlert = false
    @State private var alertMessage = ""
    @State private var showingConnectionAlert = false
    
    
    // Header component to break up complex expressions
    private var headerView: some View {
        HStack {
            VStack(alignment: .leading) {
                Text("Sistema de Extinción")
                    .font(.title2)
                    .fontWeight(.bold)
                Text("Usuario: \(firebaseService.currentUser?.email ?? "No identificado")")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            Spacer()
            
            // Indicador de estado de conexión
            ConnectionStatusIndicator(status: firebaseService.connectionStatus)
            
            Button("Cerrar Sesión") {
                firebaseService.logout()
            }
            .foregroundColor(.red)
        }
        .padding(.horizontal)
    }
    
    var body: some View {
        NavigationView {
            ScrollView(.vertical, showsIndicators: false) {
                VStack(spacing: 20) {
                    // Header con información del usuario
                    headerView
                    
                    // Header con estado del sistema
                    SystemStatusCard()
                    
                    // Estado de notificaciones
                    VStack(alignment: .leading, spacing: 8) {
                        Text("Estado de Notificaciones")
                            .font(.headline)
                            .foregroundColor(.primary)
                        
                        HStack {
                            Circle()
                                .fill(firebaseService.notificationPermissionGranted ? Color.green : Color.orange)
                                .frame(width: 12, height: 12)
                            Text(firebaseService.notificationPermissionGranted ? "Notificaciones habilitadas" : "Notificaciones deshabilitadas")
                                .font(.subheadline)
                                .foregroundColor(.secondary)
                        }
                    }
                    .padding()
                    .background(Color(.systemGray6))
                    .cornerRadius(12)
                    
                    // Token FCM (solo para desarrollo)
                    if let fcmToken = firebaseService.fcmToken {
                        VStack(alignment: .leading, spacing: 8) {
                            Text("Token FCM")
                                .font(.headline)
                                .foregroundColor(.primary)
                            
                            Text("Token para notificaciones push:")
                                .font(.subheadline)
                                .foregroundColor(.secondary)
                            
                            HStack {
                                Text(fcmToken)
                                    .font(.caption)
                                    .foregroundColor(.secondary)
                                    .lineLimit(3)
                                    .truncationMode(.middle)
                                
                                Spacer()
                                
                                Button("Copiar") {
                                    UIPasteboard.general.string = fcmToken
                                }
                                .font(.caption)
                                .foregroundColor(.blue)
                            }
                            
                            Button("Actualizar Token") {
                                firebaseService.refreshFCMToken()
                            }
                            .font(.caption)
                            .foregroundColor(.blue)
                        }
                        .padding()
                        .background(Color(.systemGray6))
                        .cornerRadius(12)
                    }
                    
                    // Sensores principales
                    SensorsSection()
                    
                    // Actuadores
                    ActuatorsSection()
                    
                    // Controles del sistema
                    SystemControlsSection()
                    
                    Spacer(minLength: 20)
                }
                .padding()
            }
            .navigationTitle("Sistema Contra Incendios")
            .navigationBarTitleDisplayMode(.large)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Cerrar Sesión") {
                        firebaseService.signOut()
                    }
                    .foregroundColor(.red)
                }
            }
            .alert("Error", isPresented: $showingAlert) {
                Button("OK") { }
            } message: {
                Text(alertMessage)
            }
            .alert("Problema de Conexión", isPresented: $showingConnectionAlert) {
                Button("Reintentar") {
                    // Trigger a reconnection by signing out and back in
                    showingConnectionAlert = false
                }
                Button("OK") { }
            } message: {
                Text(firebaseService.errorMessage ?? "Error de conexión")
            }
            .onChange(of: firebaseService.connectionStatus) { oldValue, newValue in
                if newValue == .disconnected || newValue == .noData || newValue == .error {
                    showingConnectionAlert = true
                }
            }
            .onReceive(firebaseService.$errorMessage) { errorMessage in
                if let error = errorMessage {
                    alertMessage = error
                    showingAlert = true
                }
            }
        }
    }
}

// MARK: - Componente de Estado de Conexión
struct ConnectionStatusIndicator: View {
     let status: FirebaseService.ConnectionStatus
     
     var body: some View {
         HStack(spacing: 4) {
             Circle()
                 .fill(statusColor)
                 .frame(width: 8, height: 8)
             Text(statusText)
                 .font(.caption2)
                 .foregroundColor(.secondary)
         }
         .padding(.horizontal, 8)
         .padding(.vertical, 4)
         .background(Color.gray.opacity(0.1))
         .cornerRadius(8)
     }
     
     private var statusColor: Color {
         switch status {
         case .connected:
             return .green
         case .disconnected, .noData, .error:
             return .red
         }
     }
     
     private var statusText: String {
         switch status {
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
 }


// MARK: - Componentes del Dashboard

struct SystemStatusCard: View {
    @StateObject private var firebaseService = FirebaseService.shared
    
    private func formatDate(_ dateString: String) -> String {
        let inputFormatter = DateFormatter()
        inputFormatter.dateFormat = "yyyy-MM-dd HH:mm:ss"
        
        let outputFormatter = DateFormatter()
        outputFormatter.dateStyle = .short
        outputFormatter.timeStyle = .short
        
        if let date = inputFormatter.date(from: dateString) {
            return outputFormatter.string(from: date)
        } else {
            return dateString
        }
    }
    
    var body: some View {
        VStack(spacing: 15) {
            HStack {
                Image(systemName: "shield.fill")
                    .font(.title2)
                    .foregroundColor(getSystemModeColor())
                
                VStack(alignment: .leading) {
                    Text("Estado del Sistema")
                        .font(.headline)
                        .foregroundColor(.primary)
                    
                    Text(firebaseService.getSystemModeText())
                        .font(.title2)
                        .fontWeight(.bold)
                        .foregroundColor(getSystemModeColor())
                }
                
                Spacer()
            }
            
            if let timestamp = firebaseService.systemData?.sensor_data.timestamp {
                HStack {
                    Image(systemName: "clock")
                        .font(.caption)
                        .foregroundColor(.secondary)
                    
                    Text("Última actualización: \(formatDate(timestamp))")
                        .font(.caption)
                        .foregroundColor(.secondary)
                    
                    Spacer()
                }
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .cornerRadius(12)
        .shadow(radius: 2)
    }
    
    private func getSystemModeColor() -> Color {
        let colorString = firebaseService.getSystemModeColor()
        switch colorString {
        case "green":
            return .green
        case "red":
            return .red
        case "orange":
            return .orange
        case "blue":
            return .blue
        default:
            return .gray
        }
    }
}

struct SensorsSection: View {
    @StateObject private var firebaseService = FirebaseService.shared
    
    var body: some View {
        VStack(alignment: .leading, spacing: 15) {
            Text("Sensores")
                .font(.title2)
                .fontWeight(.bold)
            
            LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 15) {
                // Temperatura
                SensorCard(
                    title: "Temperatura",
                    value: firebaseService.getFormattedTemperature(),
                    icon: "thermometer",
                    color: getTemperatureColor(),
                    status: "normal"
                )
                
                // Nivel de CO
                SensorCard(
                    title: "Nivel CO",
                    value: firebaseService.getFormattedCOLevel(),
                    icon: "smoke",
                    color: getCOColor(),
                    status: "normal"
                )
                
                // Detector de llama
                SensorCard(
                    title: "Detector Llama",
                    value: firebaseService.systemData?.sensor_data.sensors.flame_detected == true ? "DETECTADO" : "Normal",
                    icon: "flame",
                    color: firebaseService.systemData?.sensor_data.sensors.flame_detected == true ? .red : .green,
                    status: firebaseService.systemData?.sensor_data.status.fire_alarm == true ? "emergency" : "normal"
                )
                
                // Flujo de agua
                SensorCard(
                    title: "Flujo Agua",
                    value: firebaseService.getFormattedWaterFlow(),
                    icon: "drop",
                    color: .blue,
                    status: "normal"
                )
            }
        }
    }
    
    private func getTemperatureColor() -> Color {
        guard let temp = firebaseService.systemData?.sensor_data.sensors.temperature else { return .gray }
        
        if temp > 80 {
            return .red
        } else if temp > 50 {
            return .orange
        } else {
            return .green
        }
    }
    
    private func getCOColor() -> Color {
        guard let co = firebaseService.systemData?.sensor_data.sensors.co_ppm else { return .gray }
        
        if co > 500 {
            return .red
        } else if co > 100 {
            return .orange
        } else {
            return .green
        }
    }
}

struct SensorCard: View {
    let title: String
    let value: String
    let icon: String
    let color: Color
    let status: String
    
    var body: some View {
        VStack(spacing: 10) {
            HStack {
                Image(systemName: icon)
                    .font(.title2)
                    .foregroundColor(color)
                
                Spacer()
                
                Circle()
                    .fill(getStatusColor())
                    .frame(width: 8, height: 8)
            }
            
            VStack(alignment: .leading, spacing: 4) {
                Text(title)
                    .font(.caption)
                    .foregroundColor(.secondary)
                
                Text(value)
                    .font(.headline)
                    .fontWeight(.bold)
                    .foregroundColor(.primary)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding()
        .background(Color(UIColor.systemBackground))
        .cornerRadius(10)
        .shadow(radius: 2)
    }
    
    private func getStatusColor() -> Color {
        switch status {
        case "normal":
            return .green
        case "warning":
            return .orange
        case "critical", "emergency":
            return .red
        default:
            return .gray
        }
    }
}

struct ActuatorsSection: View {
    @StateObject private var firebaseService = FirebaseService.shared
    
    var body: some View {
        VStack(alignment: .leading, spacing: 15) {
            Text("Actuadores")
                .font(.title2)
                .fontWeight(.bold)
            
            HStack(spacing: 15) {
                // Alarma
                ActuatorCard(
                    title: "Alarma",
                    isActive: firebaseService.systemData?.sensor_data.actuators.alarm_active ?? false,
                    icon: "speaker.wave.3",
                    activeColor: .red
                )
                
                // Bomba
                ActuatorCard(
                    title: "Bomba",
                    isActive: firebaseService.systemData?.sensor_data.actuators.pump_active ?? false,
                    icon: "drop.circle",
                    activeColor: .blue
                )
            }
        }
    }
}

struct ActuatorCard: View {
    let title: String
    let isActive: Bool
    let icon: String
    let activeColor: Color
    
    var body: some View {
        VStack(spacing: 10) {
            Image(systemName: icon)
                .font(.title)
                .foregroundColor(isActive ? activeColor : .gray)
            
            Text(title)
                .font(.headline)
                .foregroundColor(.primary)
            
            Text(isActive ? "ACTIVO" : "Inactivo")
                .font(.caption)
                .fontWeight(.bold)
                .foregroundColor(isActive ? activeColor : .secondary)
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
                .background(isActive ? activeColor.opacity(0.2) : Color.clear)
                .cornerRadius(8)
        }
        .frame(maxWidth: .infinity)
        .padding()
        .background(Color(UIColor.systemBackground))
        .cornerRadius(10)
        .shadow(radius: 2)
    }
}

struct SystemControlsSection: View {
    @StateObject private var firebaseService = FirebaseService.shared
    
    var body: some View {
        VStack(alignment: .leading, spacing: 15) {
            Text("Controles del Sistema")
                .font(.title2)
                .fontWeight(.bold)
            
            VStack(spacing: 10) {
                // Botón de prueba del sistema
                Button(action: {
                    firebaseService.triggerSystemTest()
                }) {
                    HStack {
                        Image(systemName: "checkmark.circle")
                            .font(.title2)
                        
                        Text("Ejecutar Prueba del Sistema")
                            .font(.headline)
                        
                        Spacer()
                    }
                    .foregroundColor(.white)
                    .padding()
                    .background(Color.blue)
                    .cornerRadius(10)
                }
                
                // Botón de apagado/encendido - FIXED
                Button(action: {
                    firebaseService.toggleSystemShutdown()
                }) {
                    HStack {
                        Image(systemName: firebaseService.systemData?.sensor_data.status.shutdown == true ? "play.fill" : "stop.fill")
                        Text(firebaseService.systemData?.sensor_data.status.shutdown == true ? "Activar Sistema" : "Apagar Sistema")
                    }
                    .foregroundColor(.white)
                    .padding()
                    .background(firebaseService.systemData?.sensor_data.status.shutdown == true ? Color.green : Color.red)
                    .cornerRadius(8)
                }
            }
        }
    }
    
    private func getShutdownButtonText() -> String {
        guard let systemData = firebaseService.systemData else {
            return "Estado Desconocido"
        }
        
        return systemData.sensor_data.status.shutdown ? "Activar Sistema" : "Poner en Reposo"
    }
    
    private func getShutdownButtonColor() -> Color {
        guard let systemData = firebaseService.systemData else {
            return .gray
        }
        
        return systemData.sensor_data.status.shutdown ? .green : .orange
    }
}

struct DashboardView_Previews: PreviewProvider {
    static var previews: some View {
        DashboardView(firebaseService: FirebaseService.shared)
    }
}
