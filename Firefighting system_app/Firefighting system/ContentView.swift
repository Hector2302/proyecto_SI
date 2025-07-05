//
//  ContentView.swift
//  Firefighting system
//
//  Created by Hector Alvarado Mares on 22/05/25.
//

import SwiftUI

struct ContentView: View {
    @StateObject private var firebaseService = FirebaseService.shared
    
    var body: some View {
        Group {
            if firebaseService.isAuthenticated {
                MainTabView(firebaseService: firebaseService)
            } else {
                LoginView(firebaseService: firebaseService)
            }
        }
    }
}

struct MainTabView: View {
    @ObservedObject var firebaseService: FirebaseService
    
    var body: some View {
        TabView {
            DashboardView(firebaseService: firebaseService)
                .tabItem {
                    Image(systemName: "gauge")
                    Text("Dashboard")
                }
            
            HistoryView()
                .tabItem {
                    Image(systemName: "clock.arrow.circlepath")
                    Text("Historial")
                }
        }
    }
}

#Preview {
    ContentView()
}
