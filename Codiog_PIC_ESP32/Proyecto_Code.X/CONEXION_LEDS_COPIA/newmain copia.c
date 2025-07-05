/*
 * File:   newmain.c
 * Author: hectoralv
 *
 * Created on 27 de mayo de 2025, 12:13 PM
 * Programa para controlar un LCD 16x2 en modo 4 bits con PIC18F4550
 * y mostrar la temperatura de un sensor LM35
 */

 #include <xc.h>
 #include <stdio.h>
 #include <stdlib.h>
 
 // Configuraci�n de bits de configuraci�n
 #pragma config FOSC = HS        // Oscilador externo de alta velocidad
 #pragma config WDT = OFF        // Watchdog Timer desactivado
 #pragma config PWRT = ON        // Power-up Timer activado
 #pragma config BOR = ON         // Brown-out Reset activado
 #pragma config PBADEN = OFF     // PORTB<4:0> configurados como I/O digitales
 #pragma config LVP = OFF        // Programaci�n de bajo voltaje desactivada
 
 // Definici�n de frecuencia del oscilador
 #define _XTAL_FREQ 4000000     // 4MHz
 
 // Definici�nes de pines para LCD
 #define LCD_RS      PORTDbits.RD0    // RS pin
 #define LCD_RW      PORTDbits.RD1    // RW pin
 #define LCD_E       PORTDbits.RD2    // E pin
 #define LCD_DATA    PORTD            // Puerto de datos (RD4-RD7)
 
 // Variables globales para el sensor de flujo
 volatile unsigned int pulse_count = 0;  // Contador de pulsos
 volatile float flow_rate = 0.0;         // Flujo en L/min
 volatile float total_liters = 0.0;      // Total de litros
 volatile unsigned char alarm_active = 0; // Estado de alarma
 volatile unsigned int timer_5sec = 0;   // Contador para 5 segundos
 
 // Comandos LCD
 #define LCD_CLEAR           0x01    // Limpiar pantalla
 #define LCD_HOME            0x02    // Cursor a home
 #define LCD_ENTRY_MODE      0x06    // Incremento autom�tico
 #define LCD_DISPLAY_OFF     0x08    // Display apagado
 #define LCD_DISPLAY_ON      0x0C    // Display encendido, cursor apagado
 #define LCD_FUNCTION_RESET  0x30    // Reset en modo 8-bit
 #define LCD_FUNCTION_SET_4BIT 0x28  // 4-bit, 2 l�neas, 5x7 dots
 #define LCD_SET_CURSOR      0x80    // Direcci�n DDRAM + posici�n
 #define LCD_LINE2           0xC0    // Direcci�n de la segunda l�nea
 
 // Prototipos de funciones
 void LCD_Init(void);
 void LCD_Command(unsigned char cmd);
 void LCD_Char(unsigned char data);
 void LCD_String(const char *str);
 void LCD_SetCursor(unsigned char row, unsigned char col);
 void LCD_Clear(void);
 void ADC_Init(void);
 unsigned int ADC_Read(unsigned char channel);
 void Display_Temperature(float temp);
 void Flow_Init(void);
 void Calculate_Flow(void);
 void Display_Flow_Info(void);
 void __interrupt() ISR(void);
 
 // Funci�n principal
 void main(void) {
     // Variables para la temperatura
     unsigned int adc_value;
     float temperature;
     char temp_str[16];
     
     // Configuraci�n de puertos
     TRISD = 0x00;    // PORTD como salida
     PORTD = 0x00;    // Inicializar PORTD en 0
     TRISAbits.RA0 = 1; // RA0 como entrada para el sensor LM35
     TRISAbits.RA1 = 1; // RA1 como entrada para el sensor de flama
     TRISAbits.RA2 = 1; // RA2 como entrada para el sensor de humo
     
     // Configuraci�n del pin RB0 para el rel�
     TRISBbits.RB0 = 0; // RB0 como salida para el rel�
     PORTBbits.RB0 = 0; // Inicializar RB0 en 0 (rel� desactivado)
     
     // Configuraci�n del pin RB1 para el sensor de flujo
     TRISBbits.RB1 = 1; // RB1 como entrada para el sensor de flujo
     
     // Configuraci�n del pin RB2 para el buzzer pasivo
     TRISBbits.RB2 = 0; // RB2 como salida para el buzzer
     PORTBbits.RB2 = 0; // Inicializar RB2 en 0 (buzzer desactivado)
     
     // Inicializar sistema de flujo
     Flow_Init();
     
     // Inicializar LCD y ADC
     LCD_Init();
     ADC_Init();
     
     // Mostrar mensaje inicial
     LCD_Clear();
     LCD_SetCursor(0, 0);
     LCD_String("Temperatura:");
     
     // Bucle infinito
     while(1) {
         // Leer estado de los sensores
         unsigned char flame_detected = PORTAbits.RA1;
         unsigned char smoke_detected = PORTAbits.RA2;
         
         // Activar el rel� y buzzer si se detecta humo o fuego
         if(flame_detected || smoke_detected) {
             PORTBbits.RB0 = 1; // Activar rel�
             PORTBbits.RB2 = 1; // Activar buzzer
             if(!alarm_active) {
                 // Primera vez que se activa la alarma
                 alarm_active = 1;
                 pulse_count = 0;
                 total_liters = 0.0;
                 timer_5sec = 0;
             }
         } else {
             PORTBbits.RB0 = 0; // Desactivar rel�
             PORTBbits.RB2 = 0; // Desactivar buzzer
             alarm_active = 0;
             pulse_count = 0;
             total_liters = 0.0;
             timer_5sec = 0;
         }
         
         // Si hay alarma activa, mostrar informaci�n de flujo
         if(alarm_active) {
             // Calcular flujo cada segundo
             Calculate_Flow();
             
             // Incrementar contador de 5 segundos
             timer_5sec++;
             
             // Verificar si no hay flujo despu�s de 5 segundos
             if(timer_5sec >= 10 && flow_rate < 0.1) { // 10 * 500ms = 5 segundos
                 LCD_Clear();
                 LCD_SetCursor(0, 0);
                 LCD_String("ADVERTENCIA!");
                 LCD_SetCursor(1, 0);
                 LCD_String("Bomba no funciona");
             } else {
                 // Mostrar informaci�n de flujo
                 Display_Flow_Info();
             }
         }
         // Si no hay alertas, mostrar solo la temperatura
         else if(!flame_detected && !smoke_detected) {
             // Leer valor del ADC
             adc_value = ADC_Read(0); // Canal 0 (AN0)
             
             // Convertir a temperatura (LM35: 10mV/�C)
             // Con referencia de 5V y resoluci�n de 10 bits (1024 pasos)
             // Temperatura = (ADC_value * 5000 / 1023) / 10
             temperature = (adc_value * 0.4887); // Simplificaci�n de la f�rmula
             
             // Mostrar temperatura en LCD solo cuando no hay alertas
             LCD_Clear();
             LCD_SetCursor(0, 0);
             LCD_String("Temperatura:");
             sprintf(temp_str, "%2.1f C", temperature);
             LCD_SetCursor(1, 0);
             LCD_String(temp_str);
         }
         
         // Retardo para actualizaci�n
         __delay_ms(500);
     }
     
     return;
 }
 
 // Funci�n para inicializar el ADC
 void ADC_Init(void) {
     // Configurar ADCON1
     ADCON1 = 0x0E;  // AN0 como anal�gico, resto como digital
     
     // Configurar ADCON0
     ADCON0 = 0x01;  // Canal 0, ADC habilitado
     
     // Configurar ADCON2
     ADCON2 = 0xA6;  // Justificaci�n a la derecha, 12 TAD, Fosc/64
     
     __delay_ms(20);  // Esperar a que se estabilice
 }
 
 // Funci�n para leer el ADC
 unsigned int ADC_Read(unsigned char channel) {
     // Seleccionar canal
     ADCON0 &= 0xC3;  // Limpiar bits de selecci�n de canal
     ADCON0 |= (channel << 2);  // Establecer canal
     
     __delay_us(20);  // Esperar adquisici�n
     
     // Iniciar conversi�n
     ADCON0bits.GO = 1;
     while(ADCON0bits.GO);  // Esperar a que termine la conversi�n
     
     // Retornar resultado - corregido para evitar advertencia de conversi�n de signos
     return ((unsigned int)ADRESH << 8) | (unsigned int)ADRESL;
 }
 
 // Funci�n para enviar un nibble (4 bits) al LCD
 void LCD_Nibble(unsigned char nibble) {
     // Enviar los 4 bits superiores a RD4-RD7
     LCD_DATA &= 0x0F;                  // Limpiar bits superiores
     LCD_DATA |= (nibble & 0xF0);       // Establecer bits superiores
     
     // Pulso en E
     LCD_E = 1;
     __delay_us(1);                     // Retardo de 1us
     LCD_E = 0;
     __delay_us(100);                   // Retardo de 100us
 }
 
 // Funci�n para enviar un comando al LCD
 void LCD_Command(unsigned char cmd) {
     LCD_RS = 0;                        // Modo comando
     LCD_RW = 0;                        // Modo escritura
     
     // Enviar nibble superior
     LCD_Nibble(cmd & 0xF0);            // Enviar 4 bits superiores
     
     // Enviar nibble inferior
     LCD_Nibble((cmd << 4) & 0xF0);     // Enviar 4 bits inferiores
     
     // Si es un comando de borrado o retorno, esperar m�s tiempo
     if(cmd == LCD_CLEAR || cmd == LCD_HOME)
         __delay_ms(2);                 // Retardo de 2ms
     else
         __delay_us(40);                // Retardo de 40us
 }
 
 // Funci�n para enviar un car�cter al LCD
 void LCD_Char(unsigned char data) {
     LCD_RS = 1;                        // Modo datos
     LCD_RW = 0;                        // Modo escritura
     
     // Enviar nibble superior
     LCD_Nibble(data & 0xF0);           // Enviar 4 bits superiores
     
     // Enviar nibble inferior
     LCD_Nibble((data << 4) & 0xF0);    // Enviar 4 bits inferiores
     
     __delay_us(40);                    // Retardo de 40us
 }
 
 // Funci�n para enviar una cadena al LCD
 void LCD_String(const char *str) {
     while(*str)
         LCD_Char(*str++);              // Enviar cada car�cter
 }
 
 // Funci�n para posicionar el cursor
 void LCD_SetCursor(unsigned char row, unsigned char col) {
     unsigned char address;
     
     // Calcular direcci�n DDRAM
     if(row == 0)
         address = 0x80 + col;          // Primera l�nea
     else
         address = 0xC0 + col;          // Segunda l�nea
     
     LCD_Command(address);              // Enviar comando
 }
 
 // Funci�n para limpiar la pantalla
 void LCD_Clear(void) {
     LCD_Command(LCD_CLEAR);            // Enviar comando de limpieza
 }
 
 // Funci�n para inicializar el LCD en modo 4 bits
 void LCD_Init(void) {
     // Retardo inicial para estabilizaci�n
     __delay_ms(15);
     
     // Secuencia de inicializaci�n para modo 4 bits
     LCD_RS = 0;
     LCD_RW = 0;
     
     // Enviar 0x3 tres veces (secuencia de reset)
     LCD_Nibble(0x30);
     __delay_ms(5);
     LCD_Nibble(0x30);
     __delay_ms(5);
     LCD_Nibble(0x30);
     __delay_ms(5);
     
     // Cambiar a modo 4 bits
     LCD_Nibble(0x20);
     __delay_ms(5);
     
     // Configuraci�n final en modo 4 bits
     LCD_Command(LCD_FUNCTION_SET_4BIT); // 4-bit, 2 l�neas, 5x7 dots
     LCD_Command(LCD_DISPLAY_OFF);       // Display apagado
     LCD_Command(LCD_CLEAR);             // Limpiar pantalla
     LCD_Command(LCD_ENTRY_MODE);        // Incremento autom�tico
     LCD_Command(LCD_DISPLAY_ON);        // Display encendido, cursor apagado
 }
 
 // Funci�n para inicializar el sistema de flujo
 void Flow_Init(void) {
     // Configurar interrupci�n externa INT1 (RB1)
     INTCON2bits.INTEDG1 = 1;  // Flanco de subida
     INTCON3bits.INT1IE = 1;   // Habilitar INT1
     INTCON3bits.INT1IF = 0;   // Limpiar bandera
     INTCONbits.GIE = 1;       // Habilitar interrupciones globales
     INTCONbits.PEIE = 1;      // Habilitar interrupciones perif�ricas
 }
 
 // Funci�n para calcular el flujo
 void Calculate_Flow(void) {
     // YFS201: aproximadamente 7.5 pulsos por litro
     // Flujo en L/min = (pulsos * 60) / (7.5 * tiempo_en_segundos)
     // Como se llama cada 0.5 segundos, multiplicamos por 2
     flow_rate = (pulse_count * 60.0 * 2.0) / 7.5;
     
     // Acumular total de litros
     total_liters += (pulse_count / 7.5);
     
     // Reiniciar contador de pulsos
     pulse_count = 0;
 }
 
 // Funci�n para mostrar informaci�n de flujo
 void Display_Flow_Info(void) {
     char flow_str[16];
     char total_str[16];
     
     LCD_Clear();
     
     // Mostrar flujo en L/min
     LCD_SetCursor(0, 0);
     sprintf(flow_str, "Flujo:%2.1f L/m", flow_rate);
     LCD_String(flow_str);
     
     // Mostrar total en litros
     LCD_SetCursor(1, 0);
     sprintf(total_str, "Total:%2.1f Lts", total_liters);
     LCD_String(total_str);
 }
 
 // Rutina de interrupci�n
 void __interrupt() ISR(void) {
     // Interrupci�n externa INT1 (sensor de flujo)
     if(INTCON3bits.INT1IF) {
         if(alarm_active) {
             pulse_count++;  // Incrementar contador de pulsos
         }
         INTCON3bits.INT1IF = 0;  // Limpiar bandera
     }
 }
 