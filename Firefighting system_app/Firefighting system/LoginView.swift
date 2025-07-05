//
//  LoginView.swift
//  Firefighting system
//
//  Created by Hector Alvarado Mares on 22/05/25.
//

import SwiftUI

struct LoginView: View {
    @ObservedObject var firebaseService: FirebaseService
    @State private var email = ""
    @State private var password = ""
    @State private var showPassword = false
    @State private var showingErrorAlert = false
    
    var body: some View {
        NavigationView {
            ZStack {
                // Fondo degradado
                LinearGradient(
                    gradient: Gradient(colors: [Color.red.opacity(0.8), Color.orange.opacity(0.6)]),
                    startPoint: .topLeading,
                    endPoint: .bottomTrailing
                )
                .ignoresSafeArea()
                
                VStack(spacing: 30) {
                    Spacer()
                    
                    // Logo y título
                    VStack(spacing: 20) {
                        Image(systemName: "flame.fill")
                            .font(.system(size: 80))
                            .foregroundColor(.white)
                            .shadow(radius: 10)
                        
                        Text("Sistema Contra Incendios")
                            .font(.title)
                            .fontWeight(.bold)
                            .foregroundColor(.white)
                            .multilineTextAlignment(.center)
                    }
                    
                    Spacer()
                    
                    // Formulario de login
                    VStack(spacing: 20) {
                        // Campo de email
                        VStack(alignment: .leading, spacing: 8) {
                            Text("Correo Electrónico")
                                .font(.headline)
                                .foregroundColor(.white)
                            
                            HStack {
                                Image(systemName: "envelope.fill")
                                    .foregroundColor(.secondary)
                                
                                TextField("Ingresa tu correo", text: $email)
                                    .textFieldStyle(PlainTextFieldStyle())
                                    .keyboardType(.emailAddress)
                                    .autocapitalization(.none)
                                    .disableAutocorrection(true)
                                    .foregroundColor(.primary)
                            }
                            .padding()
                            .background(Color(.systemBackground))
                            .cornerRadius(10)
                            .shadow(radius: 5)
                        }
                        
                        // Campo de contraseña
                        VStack(alignment: .leading, spacing: 8) {
                            Text("Contraseña")
                                .font(.headline)
                                .foregroundColor(.white)
                            
                            HStack {
                                Image(systemName: "lock.fill")
                                    .foregroundColor(.secondary)
                                
                                if showPassword {
                                    TextField("Ingresa tu contraseña", text: $password)
                                        .textFieldStyle(PlainTextFieldStyle())
                                        .foregroundColor(.primary)
                                } else {
                                    SecureField("Ingresa tu contraseña", text: $password)
                                        .textFieldStyle(PlainTextFieldStyle())
                                        .foregroundColor(.primary)
                                }
                                
                                Button(action: {
                                    showPassword.toggle()
                                }) {
                                    Image(systemName: showPassword ? "eye.slash.fill" : "eye.fill")
                                        .foregroundColor(.secondary)
                                }
                            }
                            .padding()
                            .background(Color(.systemBackground))
                            .cornerRadius(10)
                            .shadow(radius: 5)
                        }
                        
                        // Mensaje de error
                        if let errorMessage = firebaseService.errorMessage {
                            Text(errorMessage)
                                .foregroundColor(.white)
                                .font(.caption)
                                .padding(.horizontal)
                                .multilineTextAlignment(.center)
                        }
                        
                        // Botón de login
                        Button(action: {
                            firebaseService.signIn(email: email, password: password)
                        }) {
                            HStack {
                                if firebaseService.isLoading {
                                    ProgressView()
                                        .progressViewStyle(CircularProgressViewStyle(tint: .white))
                                        .scaleEffect(0.8)
                                } else {
                                    Image(systemName: "arrow.right.circle.fill")
                                        .font(.title2)
                                }
                                
                                Text(firebaseService.isLoading ? "Iniciando Sesión..." : "Iniciar Sesión")
                                    .font(.headline)
                                    .fontWeight(.semibold)
                            }
                            .foregroundColor(.white)
                            .frame(maxWidth: .infinity)
                            .padding()
                            .background(
                                LinearGradient(
                                    gradient: Gradient(colors: [Color.blue, Color.purple]),
                                    startPoint: .leading,
                                    endPoint: .trailing
                                )
                            )
                            .cornerRadius(10)
                            .shadow(radius: 5)
                        }
                        .disabled(firebaseService.isLoading || email.isEmpty || password.isEmpty)
                        .opacity((firebaseService.isLoading || email.isEmpty || password.isEmpty) ? 0.6 : 1.0)
                    }
                    .padding(.horizontal, 30)
                    
                    Spacer()
                    
                    // Información adicional
                    VStack(spacing: 10) {
                        Text("Acceso Autorizado Únicamente")
                            .font(.caption)
                            .foregroundColor(.white.opacity(0.8))
                        
                        Text("Sistema de Monitoreo en Tiempo Real")
                            .font(.caption2)
                            .foregroundColor(.white.opacity(0.6))
                    }
                    .padding(.bottom, 30)
                }
            }
        }
        .navigationBarHidden(true)
        .alert("Error de Autenticación", isPresented: $showingErrorAlert) {
            Button("Reintentar") {
                // El usuario puede intentar de nuevo
            }
            Button("OK") { }
        } message: {
            Text(firebaseService.errorMessage ?? "Error desconocido")
        }
        .onChange(of: firebaseService.errorMessage) { oldValue, newValue in
            if newValue != nil {
                showingErrorAlert = true
            }
        }
    }
}

#Preview {
    LoginView(firebaseService: FirebaseService.shared)
}