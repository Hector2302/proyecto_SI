//
//  HistoryView.swift
//  Firefighting system
//
//  Created by Hector Alvarado Mares on 26/05/25.
//

import SwiftUI
#if canImport(Charts)
import Charts
#endif

struct HistoryView: View {
    @StateObject private var firestoreService = FirestoreService.shared
    @State private var selectedTab = 0
    @State private var showingDatePicker = false
    @State private var selectedStartDate = Calendar.current.date(byAdding: .day, value: -7, to: Date()) ?? Date()
    @State private var selectedEndDate = Date()
    
    var body: some View {
        NavigationView {
            VStack {
                // Selector de pestañas
                Picker("Categoría", selection: $selectedTab) {
                    Text("Sensores").tag(0)
                    Text("Eventos").tag(1)
                    Text("Errores").tag(2)
                    Text("Notificaciones").tag(3)
                }
                .pickerStyle(SegmentedPickerStyle())
                .padding()
                
                // Contenido según la pestaña seleccionada
                TabView(selection: $selectedTab) {
                    SensorHistoryTab()
                        .tag(0)
                    
                    EventsHistoryTab()
                        .tag(1)
                    
                    ErrorsHistoryTab()
                        .tag(2)
                    
                    NotificationsHistoryTab()
                        .tag(3)
                }
                .tabViewStyle(PageTabViewStyle(indexDisplayMode: .never))
            }
            .navigationTitle("Historial del Sistema")
            .navigationBarTitleDisplayMode(.large)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Actualizar") {
                        firestoreService.loadAllHistoricalData()
                    }
                }
            }
            .onAppear {
                firestoreService.loadAllHistoricalData()
            }
        }
    }
}

// MARK: - Pestaña de Historial de Sensores
struct SensorHistoryTab: View {
    @StateObject private var firestoreService = FirestoreService.shared
    
    var body: some View {
        ScrollView {
            LazyVStack(spacing: 20) {
                if firestoreService.isLoading {
                    ProgressView("Cargando datos de sensores...")
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else if firestoreService.sensorAverages.isEmpty {
                    Text("No hay datos históricos de sensores disponibles")
                        .foregroundColor(.secondary)
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else {
                    // Gráfico de temperatura
                    TemperatureChartCard()
                    
                    // Gráfico de CO
                    COLevelChartCard()
                    
                    // Gráfico de detecciones de llama
                    FlameDetectionChartCard()
                    
                    // Lista de promedios horarios
                    SensorAveragesListCard()
                }
            }
            .padding()
        }
        .refreshable {
            firestoreService.fetchSensorAverages()
        }
    }
}

// MARK: - Pestaña de Eventos
struct EventsHistoryTab: View {
    @StateObject private var firestoreService = FirestoreService.shared
    
    var body: some View {
        ScrollView {
            LazyVStack(spacing: 15) {
                if firestoreService.isLoading {
                    ProgressView("Cargando eventos...")
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else {
                    // Eventos de incendio y pruebas
                    if !firestoreService.fireAndTestEvents.isEmpty {
                        FireAndTestEventsCard()
                    }
                    
                    // Eventos de señal débil
                    if !firestoreService.weakSignalEvents.isEmpty {
                        WeakSignalEventsCard()
                    }
                    
                    // Eventos de modo del sistema
                    if !firestoreService.systemModeEvents.isEmpty {
                        SystemModeEventsCard()
                    }
                    
                    if firestoreService.fireAndTestEvents.isEmpty && 
                       firestoreService.weakSignalEvents.isEmpty && 
                       firestoreService.systemModeEvents.isEmpty {
                        Text("No hay eventos disponibles")
                            .foregroundColor(.secondary)
                            .frame(maxWidth: .infinity, maxHeight: .infinity)
                    }
                }
            }
            .padding()
        }
        .refreshable {
            firestoreService.fetchFireAndTestEvents()
            firestoreService.fetchWeakSignalEvents()
            firestoreService.fetchSystemModeEvents()
        }
    }
}

// MARK: - Pestaña de Errores
struct ErrorsHistoryTab: View {
    @StateObject private var firestoreService = FirestoreService.shared
    @State private var showResolvedErrors = false
    
    var body: some View {
        VStack {
            // Toggle para mostrar errores resueltos
            HStack {
                Text("Mostrar errores resueltos")
                Spacer()
                Toggle("", isOn: $showResolvedErrors)
                    .onChange(of: showResolvedErrors) { oldValue, newValue in
                        firestoreService.fetchSensorErrors(includeResolved: newValue)
                    }
            }
            .padding()
            
            ScrollView {
                LazyVStack(spacing: 15) {
                    if firestoreService.isLoading {
                        ProgressView("Cargando errores...")
                            .frame(maxWidth: .infinity, maxHeight: .infinity)
                    } else if firestoreService.sensorErrors.isEmpty {
                        Text("No hay errores de sensores")
                            .foregroundColor(.secondary)
                            .frame(maxWidth: .infinity, maxHeight: .infinity)
                    } else {
                        ForEach(firestoreService.sensorErrors) { error in
                            SensorErrorCard(error: error)
                        }
                    }
                }
                .padding()
            }
        }
        .refreshable {
            firestoreService.fetchSensorErrors(includeResolved: showResolvedErrors)
        }
    }
}

// MARK: - Pestaña de Notificaciones
struct NotificationsHistoryTab: View {
    @StateObject private var firestoreService = FirestoreService.shared
    @State private var showUnreadOnly = false
    
    var body: some View {
        VStack {
            // Toggle para mostrar solo no leídas
            HStack {
                Text("Solo notificaciones no leídas")
                Spacer()
                Toggle("", isOn: $showUnreadOnly)
                    .onChange(of: showUnreadOnly) { oldValue, newValue in
                        firestoreService.fetchSystemNotifications(unreadOnly: newValue)
                    }
            }
            .padding()
            
            ScrollView {
                LazyVStack(spacing: 15) {
                    if firestoreService.isLoading {
                        ProgressView("Cargando notificaciones...")
                            .frame(maxWidth: .infinity, maxHeight: .infinity)
                    } else if firestoreService.systemNotifications.isEmpty {
                        Text("No hay notificaciones")
                            .foregroundColor(.secondary)
                            .frame(maxWidth: .infinity, maxHeight: .infinity)
                    } else {
                        ForEach(firestoreService.systemNotifications) { notification in
                            SystemNotificationCard(notification: notification)
                        }
                    }
                }
                .padding()
            }
        }
        .refreshable {
            firestoreService.fetchSystemNotifications(unreadOnly: showUnreadOnly)
        }
    }
}

// MARK: - Componentes de Gráficos
struct TemperatureChartCard: View {
    @StateObject private var firestoreService = FirestoreService.shared
    
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Temperatura (Últimas 24 horas)")
                .font(.headline)
                .fontWeight(.bold)
            
            #if canImport(Charts)
            if #available(iOS 16.0, *) {
                Chart(firestoreService.sensorAverages.prefix(24)) { average in
                    LineMark(
                        x: .value("Hora", formatHour(average.timestamp)),
                        y: .value("Temperatura", average.temperature.avg)
                    )
                    .foregroundStyle(.red)
                    
                    AreaMark(
                        x: .value("Hora", formatHour(average.timestamp)),
                        yStart: .value("Min", average.temperature.min),
                        yEnd: .value("Max", average.temperature.max)
                    )
                    .foregroundStyle(.red.opacity(0.3))
                }
                .frame(height: 200)
                .chartYAxis {
                    AxisMarks(position: .leading)
                }
                .chartXAxis {
                    AxisMarks(values: .stride(by: 4)) { _ in
                        AxisGridLine()
                        AxisTick()
                        AxisValueLabel()
                    }
                }
            } else {
                fallbackTemperatureView()
            }
            #else
            fallbackTemperatureView()
            #endif
        }
        .padding()
        .background(Color(.systemBackground))
        .cornerRadius(12)
        .shadow(radius: 2)
    }
    
    private func formatHour(_ timestamp: String) -> String {
        let formatter = ISO8601DateFormatter()
        guard let date = formatter.date(from: timestamp) else { return "" }
        
        let hourFormatter = DateFormatter()
        hourFormatter.dateFormat = "HH:mm"
        return hourFormatter.string(from: date)
    }
    
    private func fallbackTemperatureView() -> some View {
        VStack {
            Text("Datos de Temperatura")
                .font(.subheadline)
                .foregroundColor(.secondary)
            
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 10) {
                    ForEach(firestoreService.sensorAverages.prefix(10)) { average in
                        VStack {
                            Text("\(String(format: "%.1f", average.temperature.avg))°C")
                                .font(.caption)
                                .fontWeight(.bold)
                            Text(formatHour(average.timestamp))
                                .font(.caption2)
                                .foregroundColor(.secondary)
                        }
                        .padding(8)
                        .background(Color(.systemGray6))
                        .cornerRadius(8)
                    }
                }
                .padding(.horizontal)
            }
        }
        .frame(height: 200)
    }
    
    private func fallbackCOView() -> some View {
        VStack {
            Text("Datos de Nivel de CO")
                .font(.subheadline)
                .foregroundColor(.secondary)
            
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 10) {
                    ForEach(firestoreService.sensorAverages.prefix(10)) { average in
                        VStack {
                            Text("\(average.coLevel.avgPpm) ppm")
                                .font(.caption)
                                .fontWeight(.bold)
                            Text(formatHour(average.timestamp))
                                .font(.caption2)
                                .foregroundColor(.secondary)
                        }
                        .padding(8)
                        .background(Color.orange.opacity(0.1))
                        .cornerRadius(8)
                    }
                }
                .padding(.horizontal)
            }
        }
        .frame(height: 200)
    }
    
    private func fallbackFlameView() -> some View {
        VStack {
            Text("Datos de Detección de Llama")
                .font(.subheadline)
                .foregroundColor(.secondary)
            
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 10) {
                    ForEach(firestoreService.sensorAverages.prefix(10)) { average in
                        VStack {
                            Text("\(average.flame.detectedCount) det.")
                                .font(.caption)
                                .fontWeight(.bold)
                                .foregroundColor(average.flame.detectedCount > 0 ? .red : .primary)
                            Text(formatHour(average.timestamp))
                                .font(.caption2)
                                .foregroundColor(.secondary)
                        }
                        .padding(8)
                        .background(Color.red.opacity(0.1))
                        .cornerRadius(8)
                    }
                }
                .padding(.horizontal)
            }
        }
        .frame(height: 200)
    }
}

struct COLevelChartCard: View {
    @StateObject private var firestoreService = FirestoreService.shared
    
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Nivel de CO (Últimas 24 horas)")
                .font(.headline)
                .fontWeight(.bold)
            
            #if canImport(Charts)
            if #available(iOS 16.0, *) {
                Chart(firestoreService.sensorAverages.prefix(24)) { average in
                    LineMark(
                        x: .value("Hora", formatHour(average.timestamp)),
                        y: .value("CO (ppm)", average.coLevel.avgPpm)
                    )
                    .foregroundStyle(.orange)
                    
                    // Línea de peligro en 100 ppm
                    RuleMark(y: .value("Peligro", 100))
                        .foregroundStyle(.red)
                        .lineStyle(StrokeStyle(lineWidth: 2, dash: [5]))
                }
                .frame(height: 200)
            } else {
                fallbackCOView()
            }
            #else
            fallbackCOView()
            #endif
        }
        .padding()
        .background(Color(.systemBackground))
        .cornerRadius(12)
        .shadow(radius: 2)
    }
    
    private func formatHour(_ timestamp: String) -> String {
        let formatter = ISO8601DateFormatter()
        guard let date = formatter.date(from: timestamp) else { return "" }
        
        let hourFormatter = DateFormatter()
        hourFormatter.dateFormat = "HH:mm"
        return hourFormatter.string(from: date)
    }
    
    private func fallbackTemperatureView() -> some View {
        VStack {
            Text("Datos de Temperatura")
                .font(.subheadline)
                .foregroundColor(.secondary)
            
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 10) {
                    ForEach(firestoreService.sensorAverages.prefix(10)) { average in
                        VStack {
                            Text("\(String(format: "%.1f", average.temperature.avg))°C")
                                .font(.caption)
                                .fontWeight(.bold)
                            Text(formatHour(average.timestamp))
                                .font(.caption2)
                                .foregroundColor(.secondary)
                        }
                        .padding(8)
                        .background(Color(.systemGray6))
                        .cornerRadius(8)
                    }
                }
                .padding(.horizontal)
            }
        }
        .frame(height: 200)
    }
    
    private func fallbackCOView() -> some View {
        VStack {
            Text("Datos de Nivel de CO")
                .font(.subheadline)
                .foregroundColor(.secondary)
            
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 10) {
                    ForEach(firestoreService.sensorAverages.prefix(10)) { average in
                        VStack {
                            Text("\(average.coLevel.avgPpm) ppm")
                                .font(.caption)
                                .fontWeight(.bold)
                            Text(formatHour(average.timestamp))
                                .font(.caption2)
                                .foregroundColor(.secondary)
                        }
                        .padding(8)
                        .background(Color.orange.opacity(0.1))
                        .cornerRadius(8)
                    }
                }
                .padding(.horizontal)
            }
        }
        .frame(height: 200)
    }
    
    private func fallbackFlameView() -> some View {
        VStack {
            Text("Datos de Detección de Llama")
                .font(.subheadline)
                .foregroundColor(.secondary)
            
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 10) {
                    ForEach(firestoreService.sensorAverages.prefix(10)) { average in
                        VStack {
                            Text("\(average.flame.detectedCount) det.")
                                .font(.caption)
                                .fontWeight(.bold)
                                .foregroundColor(average.flame.detectedCount > 0 ? .red : .primary)
                            Text(formatHour(average.timestamp))
                                .font(.caption2)
                                .foregroundColor(.secondary)
                        }
                        .padding(8)
                        .background(Color.red.opacity(0.1))
                        .cornerRadius(8)
                    }
                }
                .padding(.horizontal)
            }
        }
        .frame(height: 200)
    }
}

struct FlameDetectionChartCard: View {
    @StateObject private var firestoreService = FirestoreService.shared
    
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Detecciones de Llama (Últimas 24 horas)")
                .font(.headline)
                .fontWeight(.bold)
            
            #if canImport(Charts)
            if #available(iOS 16.0, *) {
                Chart(firestoreService.sensorAverages.prefix(24)) { average in
                    BarMark(
                        x: .value("Hora", formatHour(average.timestamp)),
                        y: .value("Detecciones", average.flame.detectedCount)
                    )
                    .foregroundStyle(.red)
                }
                .frame(height: 200)
            } else {
                fallbackFlameView()
            }
            #else
            fallbackFlameView()
            #endif
        }
        .padding()
        .background(Color(.systemBackground))
        .cornerRadius(12)
        .shadow(radius: 2)
    }
    
    private func formatHour(_ timestamp: String) -> String {
        let formatter = ISO8601DateFormatter()
        guard let date = formatter.date(from: timestamp) else { return "" }
        
        let hourFormatter = DateFormatter()
        hourFormatter.dateFormat = "HH:mm"
        return hourFormatter.string(from: date)
    }
    
    private func fallbackTemperatureView() -> some View {
        VStack {
            Text("Datos de Temperatura")
                .font(.subheadline)
                .foregroundColor(.secondary)
            
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 10) {
                    ForEach(firestoreService.sensorAverages.prefix(10)) { average in
                        VStack {
                            Text("\(String(format: "%.1f", average.temperature.avg))°C")
                                .font(.caption)
                                .fontWeight(.bold)
                            Text(formatHour(average.timestamp))
                                .font(.caption2)
                                .foregroundColor(.secondary)
                        }
                        .padding(8)
                        .background(Color(.systemGray6))
                        .cornerRadius(8)
                    }
                }
                .padding(.horizontal)
            }
        }
        .frame(height: 200)
    }
    
    private func fallbackCOView() -> some View {
        VStack {
            Text("Datos de Nivel de CO")
                .font(.subheadline)
                .foregroundColor(.secondary)
            
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 10) {
                    ForEach(firestoreService.sensorAverages.prefix(10)) { average in
                        VStack {
                            Text("\(average.coLevel.avgPpm) ppm")
                                .font(.caption)
                                .fontWeight(.bold)
                            Text(formatHour(average.timestamp))
                                .font(.caption2)
                                .foregroundColor(.secondary)
                        }
                        .padding(8)
                        .background(Color.orange.opacity(0.1))
                        .cornerRadius(8)
                    }
                }
                .padding(.horizontal)
            }
        }
        .frame(height: 200)
    }
    
    private func fallbackFlameView() -> some View {
        VStack {
            Text("Datos de Detección de Llama")
                .font(.subheadline)
                .foregroundColor(.secondary)
            
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 10) {
                    ForEach(firestoreService.sensorAverages.prefix(10)) { average in
                        VStack {
                            Text("\(average.flame.detectedCount) det.")
                                .font(.caption)
                                .fontWeight(.bold)
                                .foregroundColor(average.flame.detectedCount > 0 ? .red : .primary)
                            Text(formatHour(average.timestamp))
                                .font(.caption2)
                                .foregroundColor(.secondary)
                        }
                        .padding(8)
                        .background(Color.red.opacity(0.1))
                        .cornerRadius(8)
                    }
                }
                .padding(.horizontal)
            }
        }
        .frame(height: 200)
    }
}

// MARK: - Componentes de Listas
struct SensorAveragesListCard: View {
    @StateObject private var firestoreService = FirestoreService.shared
    
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Promedios Horarios Recientes")
                .font(.headline)
                .fontWeight(.bold)
            
            ForEach(firestoreService.sensorAverages.prefix(10)) { average in
                HStack {
                    VStack(alignment: .leading, spacing: 4) {
                        Text(firestoreService.formatTimestamp(average.timestamp))
                            .font(.caption)
                            .foregroundColor(.secondary)
                        
                        HStack(spacing: 15) {
                            VStack(alignment: .leading) {
                                Text("Temp")
                                    .font(.caption2)
                                    .foregroundColor(.secondary)
                                Text("\(average.temperature.avg, specifier: "%.1f")°C")
                                    .font(.caption)
                                    .fontWeight(.medium)
                            }
                            
                            VStack(alignment: .leading) {
                                Text("CO")
                                    .font(.caption2)
                                    .foregroundColor(.secondary)
                                Text("\(average.coLevel.avgPpm) ppm")
                                    .font(.caption)
                                    .fontWeight(.medium)
                                    .foregroundColor(average.coLevel.avgPpm > 100 ? .red : .primary)
                            }
                            
                            VStack(alignment: .leading) {
                                Text("Llama")
                                    .font(.caption2)
                                    .foregroundColor(.secondary)
                                Text("\(average.flame.detectedCount) det.")
                                    .font(.caption)
                                    .fontWeight(.medium)
                                    .foregroundColor(average.flame.detectedCount > 0 ? .red : .primary)
                            }
                        }
                    }
                    
                    Spacer()
                    
                    Text("\(average.sampleCount) muestras")
                        .font(.caption2)
                        .foregroundColor(.secondary)
                }
                .padding(.vertical, 8)
                .padding(.horizontal, 12)
                .background(Color(.systemGray6))
                .cornerRadius(8)
            }
        }
        .padding()
        .background(Color(.systemBackground))
        .cornerRadius(12)
        .shadow(radius: 2)
    }
}

struct FireAndTestEventsCard: View {
    @StateObject private var firestoreService = FirestoreService.shared
    
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Eventos de Incendio y Pruebas")
                .font(.headline)
                .fontWeight(.bold)
            
            ForEach(firestoreService.fireAndTestEvents.prefix(10)) { event in
                HStack {
                    Image(systemName: event.eventType == "fire_detected" ? "flame.fill" : "wrench.fill")
                        .foregroundColor(event.eventType == "fire_detected" ? .red : .blue)
                        .frame(width: 20)
                    
                    VStack(alignment: .leading, spacing: 2) {
                        Text(event.eventType == "fire_detected" ? "Incendio Detectado" : "Prueba del Sistema")
                            .font(.subheadline)
                            .fontWeight(.medium)
                        
                        Text(firestoreService.formatTimestamp(event.timestamp))
                            .font(.caption)
                            .foregroundColor(.secondary)
                        
                        if let data = event.data {
                            if let intensity = data.flameIntensity {
                                Text("Intensidad: \(intensity)")
                                    .font(.caption2)
                                    .foregroundColor(.secondary)
                            }
                        }
                    }
                    
                    Spacer()
                }
                .padding(.vertical, 8)
                .padding(.horizontal, 12)
                .background(Color(.systemGray6))
                .cornerRadius(8)
            }
        }
        .padding()
        .background(Color(.systemBackground))
        .cornerRadius(12)
        .shadow(radius: 2)
    }
}

struct WeakSignalEventsCard: View {
    @StateObject private var firestoreService = FirestoreService.shared
    
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Eventos de Señal Débil")
                .font(.headline)
                .fontWeight(.bold)
            
            ForEach(firestoreService.weakSignalEvents.prefix(10)) { event in
                HStack {
                    Image(systemName: event.eventType == "weak_signal_detected" ? "wifi.exclamationmark" : "wifi")
                        .foregroundColor(event.eventType == "weak_signal_detected" ? .orange : .green)
                        .frame(width: 20)
                    
                    VStack(alignment: .leading, spacing: 2) {
                        Text(event.eventType == "weak_signal_detected" ? "Señal Débil" : "Señal Restaurada")
                            .font(.subheadline)
                            .fontWeight(.medium)
                        
                        Text(firestoreService.formatTimestamp(event.timestamp))
                            .font(.caption)
                            .foregroundColor(.secondary)
                        
                        Text("\(event.signalStrengthDbm) dBm")
                            .font(.caption2)
                            .foregroundColor(.secondary)
                    }
                    
                    Spacer()
                    
                    if event.durationSeconds > 0 {
                        Text("\(event.durationSeconds)s")
                            .font(.caption2)
                            .foregroundColor(.secondary)
                    }
                }
                .padding(.vertical, 8)
                .padding(.horizontal, 12)
                .background(Color(.systemGray6))
                .cornerRadius(8)
            }
        }
        .padding()
        .background(Color(.systemBackground))
        .cornerRadius(12)
        .shadow(radius: 2)
    }
}

struct SystemModeEventsCard: View {
    @StateObject private var firestoreService = FirestoreService.shared
    
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Eventos del Sistema")
                .font(.headline)
                .fontWeight(.bold)
            
            ForEach(firestoreService.systemModeEvents.prefix(10)) { event in
                HStack {
                    Image(systemName: event.eventType == "system_shutdown" ? "power" : "power.circle.fill")
                        .foregroundColor(event.eventType == "system_shutdown" ? .red : .green)
                        .frame(width: 20)
                    
                    VStack(alignment: .leading, spacing: 2) {
                        Text(event.eventType == "system_shutdown" ? "Sistema Apagado" : "Sistema Encendido")
                            .font(.subheadline)
                            .fontWeight(.medium)
                        
                        Text(firestoreService.formatTimestamp(event.timestamp))
                            .font(.caption)
                            .foregroundColor(.secondary)
                        
                        Text("Por: \(event.initiatedBy)")
                            .font(.caption2)
                            .foregroundColor(.secondary)
                    }
                    
                    Spacer()
                    
                    if let downtime = event.downtimeSeconds {
                        Text("\(downtime)s")
                            .font(.caption2)
                            .foregroundColor(.secondary)
                    }
                }
                .padding(.vertical, 8)
                .padding(.horizontal, 12)
                .background(Color(.systemGray6))
                .cornerRadius(8)
            }
        }
        .padding()
        .background(Color(.systemBackground))
        .cornerRadius(12)
        .shadow(radius: 2)
    }
}

struct SensorErrorCard: View {
    let error: SensorError
    @StateObject private var firestoreService = FirestoreService.shared
    
    var body: some View {
        HStack {
            Image(systemName: "exclamationmark.triangle.fill")
                .foregroundColor(error.resolved ? .green : .red)
                .frame(width: 20)
            
            VStack(alignment: .leading, spacing: 2) {
                Text("Error en \(error.sensor.capitalized)")
                    .font(.subheadline)
                    .fontWeight(.medium)
                
                Text("Estado: \(error.status)")
                    .font(.caption)
                    .foregroundColor(.secondary)
                
                Text(firestoreService.formatTimestamp(error.timestamp))
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            
            Spacer()
            
            if !error.resolved {
                Button("Resolver") {
                    firestoreService.resolveSensorError(errorId: error.id)
                }
                .font(.caption)
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
                .background(Color.blue)
                .foregroundColor(.white)
                .cornerRadius(4)
            } else {
                Text("Resuelto")
                    .font(.caption)
                    .foregroundColor(.green)
                    .fontWeight(.medium)
            }
        }
        .padding(.vertical, 8)
        .padding(.horizontal, 12)
        .background(Color(.systemGray6))
        .cornerRadius(8)
    }
}

struct SystemNotificationCard: View {
    let notification: SystemNotification
    @StateObject private var firestoreService = FirestoreService.shared
    
    var body: some View {
        HStack {
            Image(systemName: priorityIcon(notification.priority))
                .foregroundColor(priorityColor(notification.priority))
                .frame(width: 20)
            
            VStack(alignment: .leading, spacing: 2) {
                Text(notification.title)
                    .font(.subheadline)
                    .fontWeight(.medium)
                
                Text(notification.message)
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .lineLimit(2)
                
                Text(firestoreService.formatTimestamp(notification.timestamp))
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            
            Spacer()
            
            VStack {
                if !notification.read {
                    Button("Marcar leída") {
                        firestoreService.markNotificationAsRead(notificationId: notification.id)
                    }
                    .font(.caption)
                    .padding(.horizontal, 8)
                    .padding(.vertical, 4)
                    .background(Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(4)
                } else {
                    Text("Leída")
                        .font(.caption)
                        .foregroundColor(.green)
                        .fontWeight(.medium)
                }
                
                Circle()
                    .fill(notification.read ? Color.clear : Color.blue)
                    .frame(width: 8, height: 8)
            }
        }
        .padding(.vertical, 8)
        .padding(.horizontal, 12)
        .background(Color(.systemGray6))
        .cornerRadius(8)
    }
    
    private func priorityIcon(_ priority: String) -> String {
        switch priority.lowercased() {
        case "high", "critical":
            return "exclamationmark.triangle.fill"
        case "medium":
            return "info.circle.fill"
        default:
            return "bell.fill"
        }
    }
    
    private func priorityColor(_ priority: String) -> Color {
        switch priority.lowercased() {
        case "high", "critical":
            return .red
        case "medium":
            return .orange
        default:
            return .blue
        }
    }
}

#Preview {
    HistoryView()
}