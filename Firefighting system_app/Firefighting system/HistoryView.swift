import SwiftUI
import Charts

struct HistoryView: View {
    @StateObject private var firestoreService = FirestoreService.shared
    @State private var selectedTab = 0
    
    var body: some View {
        NavigationView {
            VStack {
                // Selector de pestañas
                Picker("Categoría", selection: $selectedTab) {
                    Text("Incendios").tag(0)
                    Text("Pruebas").tag(1)
                    Text("Comandos").tag(2)
                }
                .pickerStyle(SegmentedPickerStyle())
                .padding()
                
                // Contenido según la pestaña seleccionada
                TabView(selection: $selectedTab) {
                    FireEventsTab()
                        .tag(0)
                    
                    TestEventsTab()
                        .tag(1)
                    
                    SystemCommandsTab()
                        .tag(2)
                }
                .tabViewStyle(PageTabViewStyle(indexDisplayMode: .never))
            }
            .navigationTitle("Historial del Sistema")
            .onAppear {
                loadData()
            }
            .refreshable {
                loadData()
            }
        }
    }
    
    private func loadData() {
        firestoreService.fetchFireEvents()
        firestoreService.fetchTestEvents()
        firestoreService.fetchSystemStatusEvents()
    }
}

// MARK: - Pestaña de Eventos de Incendio
struct FireEventsTab: View {
    @StateObject private var firestoreService = FirestoreService.shared
    
    var body: some View {
        ScrollView {
            LazyVStack(spacing: 20) {
                if firestoreService.isLoading {
                    ProgressView("Cargando eventos de incendio...")
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else if firestoreService.fireEvents.isEmpty {
                    Text("No hay eventos de incendio registrados")
                        .foregroundColor(.secondary)
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else {
                    // Gráfica de eventos de incendio por día
                    FireEventsChart(events: firestoreService.fireEvents)
                    
                    // Lista de eventos
                    ForEach(firestoreService.fireEvents) { event in
                        FireEventCard(event: event)
                    }
                }
            }
            .padding()
        }
    }
}

// MARK: - Pestaña de Eventos de Prueba
struct TestEventsTab: View {
    @StateObject private var firestoreService = FirestoreService.shared
    
    var body: some View {
        ScrollView {
            LazyVStack(spacing: 20) {
                if firestoreService.isLoading {
                    ProgressView("Cargando eventos de prueba...")
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else if firestoreService.testEvents.isEmpty {
                    Text("No hay eventos de prueba registrados")
                        .foregroundColor(.secondary)
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else {
                    // Gráfica de pruebas por día
                    TestEventsChart(events: firestoreService.testEvents)
                    
                    // Lista de eventos
                    ForEach(firestoreService.testEvents) { event in
                        TestEventCard(event: event)
                    }
                }
            }
            .padding()
        }
    }
}

// MARK: - Pestaña de Comandos del Sistema
struct SystemCommandsTab: View {
    @StateObject private var firestoreService = FirestoreService.shared
    
    var body: some View {
        ScrollView {
            LazyVStack(spacing: 20) {
                if firestoreService.isLoading {
                    ProgressView("Cargando comandos del sistema...")
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else if firestoreService.systemStatusEvents.isEmpty {
                    Text("No hay comandos registrados")
                        .foregroundColor(.secondary)
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else {
                    // Gráfica de comandos por día
                    SystemCommandsChart(events: firestoreService.systemStatusEvents)
                    
                    // Lista de comandos
                    ForEach(firestoreService.systemStatusEvents) { event in
                        SystemCommandCard(event: event)
                    }
                }
            }
            .padding()
        }
    }
}

// MARK: - Tarjetas de Eventos
struct FireEventCard: View {
    let event: FireEvent
    
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text(event.eventDescription)
                    .font(.headline)
                    .foregroundColor(.primary)
                
                Spacer()
                
                Text(event.formattedDate)
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            
            if let triggerSensor = event.triggerSensor {
                Text("Sensor: \(triggerSensor)")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            }
            
            if let duration = event.durationSeconds {
                Text("Duración: \(duration) segundos")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            }
            
            if let waterUsed = event.waterUsed {
                Text("Agua utilizada: \(String(format: "%.1f", waterUsed)) L")
                    .font(.subheadline)
                    .foregroundColor(.blue)
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .cornerRadius(12)
    }
}

struct TestEventCard: View {
    let event: TestEvent
    
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text(event.eventDescription)
                    .font(.headline)
                    .foregroundColor(.primary)
                
                Spacer()
                
                Text(event.formattedDate)
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            
            if let duration = event.durationSeconds {
                Text("Duración: \(duration) segundos")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .cornerRadius(12)
    }
}

struct SystemCommandCard: View {
    let event: SystemStatusEvent
    
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text(event.eventDescription)
                    .font(.headline)
                    .foregroundColor(.primary)
                
                Spacer()
                
                Text(event.formattedDate)
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            
            if let duration = event.durationSeconds {
                Text("Duración: \(duration) segundos")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .cornerRadius(12)
    }
}

// MARK: - Gráficas
struct FireEventsChart: View {
    let events: [FireEvent]
    
    var body: some View {
        VStack(alignment: .leading) {
            Text("Eventos de Incendio por Día")
                .font(.headline)
                .padding(.bottom, 8)
            
            if #available(iOS 16.0, *) {
                Chart(eventsByDay, id: \.date) { item in
                    BarMark(
                        x: .value("Día", item.date),
                        y: .value("Eventos", item.count)
                    )
                    .foregroundStyle(.red)
                }
                .frame(height: 200)
            } else {
                Text("Gráficas disponibles en iOS 16+")
                    .foregroundColor(.secondary)
                    .frame(height: 200)
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .cornerRadius(12)
    }
    
    private var eventsByDay: [(date: Date, count: Int)] {
        let formatter = ISO8601DateFormatter()
        let calendar = Calendar.current
        
        let groupedEvents = Dictionary(grouping: events) { event in
            guard let date = formatter.date(from: event.timestamp) else { return Date() }
            return calendar.startOfDay(for: date)
        }
        
        return groupedEvents.map { (date: $0.key, count: $0.value.count) }
            .sorted { $0.date < $1.date }
    }
}

struct TestEventsChart: View {
    let events: [TestEvent]
    
    var body: some View {
        VStack(alignment: .leading) {
            Text("Pruebas del Sistema por Día")
                .font(.headline)
                .padding(.bottom, 8)
            
            if #available(iOS 16.0, *) {
                Chart(eventsByDay, id: \.date) { item in
                    BarMark(
                        x: .value("Día", item.date),
                        y: .value("Pruebas", item.count)
                    )
                    .foregroundStyle(.orange)
                }
                .frame(height: 200)
            } else {
                Text("Gráficas disponibles en iOS 16+")
                    .foregroundColor(.secondary)
                    .frame(height: 200)
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .cornerRadius(12)
    }
    
    private var eventsByDay: [(date: Date, count: Int)] {
        let formatter = ISO8601DateFormatter()
        let calendar = Calendar.current
        
        let groupedEvents = Dictionary(grouping: events) { event in
            guard let date = formatter.date(from: event.timestamp) else { return Date() }
            return calendar.startOfDay(for: date)
        }
        
        return groupedEvents.map { (date: $0.key, count: $0.value.count) }
            .sorted { $0.date < $1.date }
    }
}

struct SystemCommandsChart: View {
    let events: [SystemStatusEvent]
    
    var body: some View {
        VStack(alignment: .leading) {
            Text("Comandos del Sistema por Día")
                .font(.headline)
                .padding(.bottom, 8)
            
            if #available(iOS 16.0, *) {
                Chart(eventsByDay, id: \.date) { item in
                    BarMark(
                        x: .value("Día", item.date),
                        y: .value("Comandos", item.count)
                    )
                    .foregroundStyle(.blue)
                }
                .frame(height: 200)
            } else {
                Text("Gráficas disponibles en iOS 16+")
                    .foregroundColor(.secondary)
                    .frame(height: 200)
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .cornerRadius(12)
    }
    
    private var eventsByDay: [(date: Date, count: Int)] {
        let formatter = ISO8601DateFormatter()
        let calendar = Calendar.current
        
        let groupedEvents = Dictionary(grouping: events) { event in
            guard let date = formatter.date(from: event.timestamp) else { return Date() }
            return calendar.startOfDay(for: date)
        }
        
        return groupedEvents.map { (date: $0.key, count: $0.value.count) }
            .sorted { $0.date < $1.date }
    }
}