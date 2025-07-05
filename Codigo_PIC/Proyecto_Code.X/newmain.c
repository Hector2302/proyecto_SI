/*
 * File:   newmain.c
 * Author: hectoralv
 *
 * Programa para comunicación UART entre PIC18F4550 y ESP32
 * Sistema anti-incendios con control de actuadores
 */

#include <xc.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// Prototipos de funciones
void ADC_Init(void);
unsigned int ADC_Read(unsigned char channel);
unsigned int ADC_Read_Average(unsigned char channel, unsigned char samples);
void Calibrate_Flame_Sensor(void);
void Calibrate_MQ2(void);
void Read_LM35(void);
void Read_Flame_Sensor(void);
void Read_MQ2_Sensor(void);
void Send_Sensor_Data(void);
void UART_Init(void);
void UART_Write(char data);
void UART_Write_Text(char *text);
char UART_Read(void);
char UART_Data_Ready(void);

// Configuración de fusibles
#pragma config FOSC = INTOSCIO_EC // Oscilador interno
#pragma config WDT = OFF         // Watchdog Timer apagado
#pragma config PWRT = ON         // Power-up Timer encendido
#pragma config MCLRE = ON        // MCLR habilitado
#pragma config LVP = OFF         // Low Voltage Programming deshabilitado
#pragma config XINST = OFF       // Extended Instruction Set deshabilitado

// Definición de frecuencia para delays
#define _XTAL_FREQ 8000000  // 8MHz

// Definición de comandos para control del LED (mantenidos pero no usados para actuadores)
#define CMD_LED_ON  '1'     // Comando para encender LED
#define CMD_LED_OFF '0'     // Comando para apagar LED

// Definición de pines
#define PUMP_PIN     PORTBbits.RB0  // Rele para bomba de agua en RB0
#define ALARM_PIN    PORTBbits.RB5  // LED de alarma en RB5

// Definiciones para bool
#define false 0
#define true 1
typedef unsigned char bool;

// Canales ADC para sensores
#define LM35_CHANNEL    0   // RA0 - Sensor de temperatura LM35
#define FLAME_CHANNEL   1   // RA1 - Sensor de flama
#define MQ2_CHANNEL     2   // RA2 - Sensor de humo MQ2

// Umbrales de alarma
#define CO_ALARM_THRESHOLD 50.0  // ppm de CO para activar alarma
#define TEMP_ALARM_THRESHOLD 40.0 // Temperatura para activar alarma

// Variables globales para sensores
float temperature = 0.0;
float flame_intensity = 0.0;
float flame_base_voltage = 0.0;
bool flame_calibrated = false;
bool flame_detected = false;
float co_ppm = 0.0;
float MQ2_Ro = 10.0; 

// Variables globales para actuadores
bool pump_active = false;
bool alarm_active = false;

// Historial para filtrado MQ2
#define MQ2_HISTORY_SIZE 5
float mq2_history[MQ2_HISTORY_SIZE] = {0};
unsigned char mq2_history_index = 0;

// Intervalos de recalibración (en ms)
#define MQ2_RECALIBRATION_INTERVAL 300000  // 5 minutos (300000 ms)
#define FLAME_RECALIBRATION_INTERVAL 600000 // 10 minutos (600000 ms)
unsigned long calibrationCounter = 0;

/**
 * Inicializa el módulo ADC
 */
void ADC_Init(void) {
    // Configurar pines analógicos
    TRISA0 = 1;  // RA0 como entrada (LM35)
    TRISA1 = 1;  // RA1 como entrada (Sensor de flama)
    TRISA2 = 1;  // RA2 como entrada (MQ2)
    
    // Configurar ADC
    ADCON0bits.ADON = 1;    // Encender ADC
    ADCON1 = 0x0C;          // RA0, RA1, RA2 como analógicos, resto digitales
    ADCON2 = 0xBE;          // Right justified, 20 TAD, FOSC/32
}

/**
 * Lee un canal ADC
 * @param channel Canal a leer (0-2)
 * @return Valor ADC de 10 bits
 */
unsigned int ADC_Read(unsigned char channel) {
    ADCON0bits.CHS = channel;   // Seleccionar canal
    __delay_us(20);             // Tiempo de adquisición
    ADCON0bits.GO = 1;          // Iniciar conversión
    while(ADCON0bits.GO);       // Esperar fin de conversión
    return (ADRESH << 8) | ADRESL;  // Retornar resultado de 10 bits
}

/**
 * Lee un canal ADC promediando varias muestras
 * @param channel Canal a leer
 * @param samples Número de muestras a promediar (máx 255)
 * @return Valor promedio ADC de 10 bits
 */
unsigned int ADC_Read_Average(unsigned char channel, unsigned char samples) {
    unsigned long sum = 0;
    for(unsigned char i = 0; i < samples; i++) {
        ADCON0bits.CHS = channel;
        __delay_us(20);
        ADCON0bits.GO = 1;
        while(ADCON0bits.GO);
        sum += (ADRESH << 8) | ADRESL;
    }
    return (unsigned int)(sum / samples);
}

/**
 * Calibra el sensor de flama (establece el voltaje base)
 */
void Calibrate_Flame_Sensor() {
    // Tomar 10 muestras para calibración inicial
    float sum = 0;
    for(unsigned char i = 0; i < 10; i++) {
        unsigned int adc_value = ADC_Read(FLAME_CHANNEL);
        sum += (adc_value * 5.0) / 1024.0;
        __delay_ms(100);
    }
    flame_base_voltage = sum / 10.0;
    flame_calibrated = true;
    flame_detected = false;
    flame_intensity = 0.0;
    
    // Mensaje de calibración completada
    UART_Write_Text("Calibracion flama completada. Voltaje base: ");
    char cal_msg[30];
    sprintf(cal_msg, "%.2fV\n", flame_base_voltage);
    UART_Write_Text(cal_msg);
}

/**
 * Calibra el sensor MQ2 (establece Ro)
 */
void Calibrate_MQ2() {
    // Tomar 50 muestras durante 10 segundos
    float sum = 0;
    for(unsigned char i = 0; i < 50; i++) {
        unsigned int adc_value = ADC_Read(MQ2_CHANNEL);
        float voltage = adc_value * (5.0 / 1024.0);
        float Rs = (5.0 - voltage) / voltage;  // Rs en aire limpio
        sum += Rs;
        __delay_ms(200);
    }
    MQ2_Ro = sum / 50.0;
    
    // Mensaje de calibración completada
    UART_Write_Text("Calibracion MQ2 completada. Ro: ");
    char cal_msg[30];
    sprintf(cal_msg, "%.2f\n", MQ2_Ro);
    UART_Write_Text(cal_msg);
}

/**
 * Lee el sensor LM35 y calcula temperatura
 */
void Read_LM35(void) {
    unsigned int adc_value = ADC_Read_Average(LM35_CHANNEL, 5);
    // LM35: 10mV/°C, Vref = 5V, ADC = 10 bits
    temperature = (adc_value * 5.0 / 1024.0) / 0.01;
}

/**
 * Lee el sensor de flama (Lógica corregida con histéresis)
 */
void Read_Flame_Sensor(void) {
    if(!flame_calibrated) {
        Calibrate_Flame_Sensor();
        return;
    }
    
    unsigned int adc_value = ADC_Read_Average(FLAME_CHANNEL, 5);
    float voltage = (adc_value * 5.0) / 1024.0;
    
    // Calcular intensidad (0-100%)
    float intensity = (flame_base_voltage - voltage) * 100.0 / flame_base_voltage;
    if(intensity < 0) intensity = 0;
    if(intensity > 100) intensity = 100;
    
    // Lógica de detección con histéresis
    #define DETECTION_THRESHOLD 10.0  // Umbral de detección (10% de intensidad)
    #define RELEASE_THRESHOLD 5.0     // Umbral de liberación (5% de intensidad)
    
    static bool last_detected = false;
    
    if (!last_detected) {
        // Si no estaba detectado y supera el umbral de detección
        if (intensity >= DETECTION_THRESHOLD) {
            flame_detected = true;
            last_detected = true;
        }
    } else {
        // Si estaba detectado y cae por debajo del umbral de liberación
        if (intensity <= RELEASE_THRESHOLD) {
            flame_detected = false;
            last_detected = false;
        }
    }
    
    // Actualizar intensidad solo si hay detección
    if(flame_detected) {
        flame_intensity = intensity;
    } else {
        flame_intensity = 0.0;
    }
}

/**
 * Lee el sensor MQ2 y calcula PPM de CO (FUNCIÓN MEJORADA)
 */
void Read_MQ2_Sensor(void) {
    static bool calibrated = false;
    if(!calibrated) {
        Calibrate_MQ2();
        calibrated = true;
    }
    
    unsigned int adc_value = ADC_Read_Average(MQ2_CHANNEL, 5);
    float voltage = (adc_value * 5.0) / 1024.0;
    
    // Calcular resistencia del sensor (Rs)
    float Rs = (5.0 - voltage) / voltage;
    
    // Compensación de temperatura
    float temp_factor = 1.0 + 0.02 * (temperature - 20.0);
    Rs = Rs / temp_factor;
    
    // Calcular relación Rs/Ro
    float rs_ro_ratio = Rs / MQ2_Ro;
    
    // MEJORA: Función de transferencia mejorada para CO
    float ppm = 0.0;
    
    // Para valores muy bajos de Rs/Ro (alta concentración)
    if(rs_ro_ratio < 0.1) {
        ppm = 5000.0 * pow(rs_ro_ratio, -1.0);
    }
    // Para rango medio
    else if(rs_ro_ratio < 0.5) {
        ppm = 1000.0 * pow(rs_ro_ratio, -1.5);
    }
    // Para rango alto (baja concentración)
    else {
        ppm = 100.0 * pow(rs_ro_ratio, -2.0);
    }
    
    // Limitar valores
    if(ppm < 0) ppm = 0;
    if(ppm > 10000) ppm = 10000;
    
    // MEJORA: Filtrado de ruido con promedio móvil
    mq2_history[mq2_history_index] = ppm;
    mq2_history_index = (mq2_history_index + 1) % MQ2_HISTORY_SIZE;
    
    // Calcular promedio
    float sum = 0;
    for(int i = 0; i < MQ2_HISTORY_SIZE; i++) {
        sum += mq2_history[i];
    }
    co_ppm = sum / MQ2_HISTORY_SIZE;
}

/**
 * Envía datos de sensores por UART en formato JSON
 */
void Send_Sensor_Data(void) {
    char buffer[250];
    
    // Formatear datos en JSON
    sprintf(buffer, "{\"temp\":%.1f,\"flame_det\":%d,\"flame_int\":%.1f,\"co_ppm\":%.1f,\"pump\":%d,\"alarm\":%d}\r\n",
            temperature, 
            flame_detected ? 1 : 0,
            flame_intensity,
            co_ppm,
            pump_active ? 1 : 0,
            alarm_active ? 1 : 0);
    
    UART_Write_Text(buffer);
}

/**
 * Inicializa el módulo UART
 */
void UART_Init(void) {
    // Configurar pines
    TRISC6 = 0;  // RC6/TX como salida
    TRISC7 = 1;  // RC7/RX como entrada
    
    // Configurar UART
    SPBRG = 51;              // 9600 bps @ 8MHz (BRGH=0)
    TXSTAbits.BRGH = 1;      // Baja velocidad
    TXSTAbits.SYNC = 0;      // Modo asíncrono
    TXSTAbits.TXEN = 1;      // Habilitar transmisión
    RCSTAbits.SPEN = 1;      // Habilitar puerto serial
    RCSTAbits.CREN = 1;      // Habilitar recepción continua
}

void UART_Write(char data) {
    while(!TXSTAbits.TRMT);  // Esperar que el buffer esté vacío
    TXREG = data;            // Enviar dato
}

void UART_Write_Text(char *text) {
    while(*text) {
        UART_Write(*text++);
    }
}

char UART_Read(void) {
    if(PIR1bits.RCIF) {
        return RCREG;
    }
    return 0;
}

char UART_Data_Ready(void) {
    return PIR1bits.RCIF;
}

void main(void) {
    // Configurar oscilador interno a 8MHz
    OSCCON = 0x70;
    
    // Configurar puerto B
    TRISB = 0x00;      // Todo el puerto B como salida
    PORTB = 0x00;      // Inicializar en 0
    ALARM_PIN = 1;     // LED encendido al inicio (estado normal)
    PUMP_PIN = 0;      // Bomba apagada inicialmente
    
    // MEJORA: Precalentamiento extendido para MQ2 (30 segundos)
    UART_Write_Text("Precalentando sensores...\r\n");
    for(int i = 0; i < 30; i++) {
        __delay_ms(1000);
        UART_Write('.');
    }
    UART_Write_Text("\r\n");
    
    // Inicializar módulos
    UART_Init();
    ADC_Init();
    
    // Calibración inicial
    Calibrate_Flame_Sensor();
    Calibrate_MQ2();
    
    // Mensaje de inicio
    UART_Write_Text("PIC18F4550 iniciado con sistema anti-incendios\r\n");
    
    char received_char;
    unsigned int sensor_counter = 0;
    unsigned int alarm_blink_counter = 0;
    
    while(1) {
        // Verificar si hay datos recibidos
        if(UART_Data_Ready()) {
            received_char = UART_Read();
            
            // Procesar comando recibido (mantenido por compatibilidad)
            if(received_char == CMD_LED_ON) {
                UART_Write_Text("Comando LED ON recibido (sin efecto en actuadores)\r\n");
            }
            else if(received_char == CMD_LED_OFF) {
                UART_Write_Text("Comando LED OFF recibido (sin efecto en actuadores)\r\n");
            }
            else if(received_char != 0) {
                UART_Write_Text("Comando no reconocido. Use 1=ON, 0=OFF\r\n");
            }
        }
        
        // Leer sensores cada 250ms (4 veces por segundo)
        Read_LM35();
        Read_Flame_Sensor();
        Read_MQ2_Sensor();
        
        // Determinar estado de alarma
        bool fire_alarm = flame_detected || 
                         (co_ppm >= CO_ALARM_THRESHOLD) || 
                         (temperature >= TEMP_ALARM_THRESHOLD);
        
        // Control de actuadores
        if(fire_alarm) {
            // Activar bomba
            PUMP_PIN = 1;
            pump_active = true;
            
            // Parpadear alarma (cada 500ms)
            alarm_blink_counter++;
            if(alarm_blink_counter >= 2) { // 2 * 250ms = 500ms
                ALARM_PIN = !ALARM_PIN;
                alarm_blink_counter = 0;
            }
            alarm_active = true;
        } else {
            // Desactivar bomba
            PUMP_PIN = 0;
            pump_active = false;
            
            // LED encendido fijo (sin alarma)
            ALARM_PIN = 1;
            alarm_active = false;
            alarm_blink_counter = 0;
        }

        // Enviar datos de sensores cada 1 segundo (1000ms)
        if(sensor_counter >= 4) { // 4 * 250ms = 1000ms
            Send_Sensor_Data();
            sensor_counter = 0;
            
            // Recalibración periódica
            calibrationCounter += 1000; // Incremento de 1 segundo
            if(calibrationCounter >= MQ2_RECALIBRATION_INTERVAL) {
                Calibrate_MQ2();
                calibrationCounter = 0;
            }
            else if(calibrationCounter >= FLAME_RECALIBRATION_INTERVAL) {
                Calibrate_Flame_Sensor();
            }
        }
        
        sensor_counter++;
        __delay_ms(250);  // Lecturas cada 250ms
    }
}