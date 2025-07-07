/*
 * Sistema anti-incendios optimizado - PIC18F4550
 * Historial específico para eventos críticos
 */

#include <xc.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#pragma config FOSC = INTOSCIO_EC, WDT = OFF, PWRT = ON, MCLRE = ON, LVP = OFF, XINST = OFF

#define _XTAL_FREQ 8000000
#define PUMP_PIN     PORTBbits.RB0
#define ALARM_PIN    PORTBbits.RB5

// Definiciones para sensores
#define false 0
#define true 1
typedef unsigned char bool;

#define LM35_CHANNEL    0
#define FLAME_CHANNEL   1
#define MQ2_CHANNEL     2

// Umbrales optimizados
#define CO_ALARM_THRESHOLD 50.0
#define TEMP_ALARM_THRESHOLD 40.0
#define FLAME_DETECTION_THRESHOLD 15.0
#define FLAME_HYSTERESIS 3.0

// Sensor de flujo
#define FLOW_PULSES_PER_LITER 450

// Variables globales
float temperature = 0.0;
float flame_intensity = 0.0;
float flame_base_voltage = 0.0;
bool flame_detected = false;
float co_ppm = 0.0;
float MQ2_Ro = 10.0;
float flow_rate = 0.0;
float total_flow = 0.0;
volatile unsigned int pulse_count = 0;
bool pump_active = false;
bool alarm_active = false;
bool fire_alarm = false;
bool prev_fire_alarm = false;

// Filtro simplificado
#define FILTER_SIZE 5
float temp_samples[FILTER_SIZE] = {25.0, 25.0, 25.0, 25.0, 25.0};
float co_samples[FILTER_SIZE] = {0.0, 0.0, 0.0, 0.0, 0.0};
unsigned char sample_index = 0;

// Comandos
bool shutdown_system = false;
bool trigger_test = false;
unsigned long test_start_time = 0;
const unsigned long TEST_DURATION = 10000;

// Tiempo del sistema
unsigned long system_millis = 0;

// Variables de historial
unsigned long fire_start_time = 0;
float fire_start_temp = 0.0;
float fire_start_flame = 0.0;
float fire_start_co = 0.0;
float fire_start_flow = 0.0;
char fire_trigger_sensor[20] = "";

unsigned long test_start_flow = 0.0;

// Prototipos
void ADC_Init(void);
unsigned int ADC_Read(unsigned char channel);
void Read_Sensors(void);
void Update_Actuators(void);
void Send_Data(void);
void UART_Init(void);
void Interrupt_Init(void);
void Handle_Commands(void);
void Handle_Fire_Events(void);
void Handle_Test_Events(void);
void Handle_Shutdown_Events(void);
void Send_History_Event(const char* event_type, const char* extra_data);
float Apply_Filter(float new_value, float* samples);

// Interrupción optimizada
void __interrupt(high_priority) HighISR(void) {
    if (INTCON3bits.INT1IF) {
        pulse_count++;
        INTCON3bits.INT1IF = 0;
    }
}

void main(void) {
    // Configuración inicial
    OSCCON = 0x70;
    TRISB = 0x02;
    PORTB = 0x00;
    ALARM_PIN = 1;
    PUMP_PIN = 0;
    
    __delay_ms(2000);
    UART_Init();
    ADC_Init();
    Interrupt_Init();
    
    // Calibración inicial
    for(int i = 0; i < 50; i++) {
        unsigned int adc_value = ADC_Read(FLAME_CHANNEL);
        flame_base_voltage += (adc_value * 5.0) / 1024.0;
        __delay_ms(100);
    }
    flame_base_voltage /= 50.0;
    
    // Calibrar MQ2
    for(int i = 0; i < 50; i++) {
        unsigned int adc_value = ADC_Read(MQ2_CHANNEL);
        float voltage = (adc_value * 5.0) / 1024.0;
        MQ2_Ro += (5.0 - voltage) / voltage;
        __delay_ms(100);
    }
    MQ2_Ro /= 50.0;
    
    unsigned int cycle_count = 0;
    
    while(1) {
        system_millis += 250;
        
        Handle_Commands();
        
        if(!shutdown_system) {
            Read_Sensors();
            if(!trigger_test) {
                Update_Actuators();
            }
        }
        
        Handle_Fire_Events();
        Handle_Test_Events();
        Handle_Shutdown_Events();
        
        // Enviar datos cada 1 segundo
        if(cycle_count >= 4) {
            Send_Data();
            cycle_count = 0;
        }
        
        cycle_count++;
        sample_index = (sample_index + 1) % FILTER_SIZE;
        __delay_ms(250);
    }
}

void Handle_Commands(void) {
    if(PIR1bits.RCIF) {
        char cmd = RCREG;
        switch(cmd) {
            case 'T': // Test
                if(!trigger_test) {
                    trigger_test = true;
                    test_start_time = system_millis;
                    test_start_flow = total_flow;
                }
                break;
            case 'S': // Shutdown
                shutdown_system = true;
                break;
            case 'R': // Resume
                shutdown_system = false;
                break;
        }
    }
    
    // Manejar test
    if(trigger_test) {
        PUMP_PIN = 1;
        pump_active = true;
        
        // Parpadear alarma
        static unsigned long last_blink = 0;
        if(system_millis - last_blink >= 500) {
            ALARM_PIN = !ALARM_PIN;
            last_blink = system_millis;
        }
        alarm_active = true;
        
        // Finalizar test
        if((system_millis - test_start_time) >= TEST_DURATION) {
            trigger_test = false;
            PUMP_PIN = 0;
            ALARM_PIN = 1;
            pump_active = false;
            alarm_active = false;
        }
    }
}

void Handle_Fire_Events(void) {
    // Detectar inicio de incendio
    if(fire_alarm && !prev_fire_alarm) {
        fire_start_time = system_millis;
        fire_start_temp = temperature;
        fire_start_flame = flame_intensity;
        fire_start_co = co_ppm;
        fire_start_flow = total_flow;
        
        // Determinar qué sensor activó la alarma
        if(flame_detected) {
            strcpy(fire_trigger_sensor, "flame_sensor");
        } else if(temperature >= TEMP_ALARM_THRESHOLD) {
            strcpy(fire_trigger_sensor, "temperature_sensor");
        } else if(co_ppm >= CO_ALARM_THRESHOLD) {
            strcpy(fire_trigger_sensor, "co_sensor");
        }
        
        // Enviar evento de inicio de incendio
        char start_data[100];
        sprintf(start_data, ",\"sensor\":\"%s\"", fire_trigger_sensor);
        Send_History_Event("fire_start", start_data);
    }
    
    // Detectar fin de incendio
    if(!fire_alarm && prev_fire_alarm) {
        unsigned long duration = (system_millis - fire_start_time) / 1000; // en segundos
        float water_used = total_flow - fire_start_flow;
        
        char end_data[150];
        sprintf(end_data, ",\"duration\":%lu,\"water\":%.2f", duration, water_used);
        Send_History_Event("fire_end", end_data);
        
        // Reset flujo después de 5 segundos
        __delay_ms(5000);
        total_flow = 0.0;
        pulse_count = 0;
    }
    
    prev_fire_alarm = fire_alarm;
}

void Handle_Test_Events(void) {
    static bool prev_test = false;
    
    // Detectar inicio de test
    if(trigger_test && !prev_test) {
        Send_History_Event("test_start", "");
    }
    
    // Detectar fin de test
    if(!trigger_test && prev_test) {
        float water_used = total_flow - test_start_flow;
        
        char test_data[50];
        sprintf(test_data, ",\"water\":%.2f", water_used);
        Send_History_Event("test_end", test_data);
        
        // Reset flujo después de 3 segundos
        __delay_ms(3000);
        total_flow = 0.0;
        pulse_count = 0;
    }
    
    prev_test = trigger_test;
}

void Handle_Shutdown_Events(void) {
    static bool prev_shutdown = false;
    
    // Detectar shutdown
    if(shutdown_system && !prev_shutdown) {
        Send_History_Event("shutdown", "");
    }
    
    // Detectar resume
    if(!shutdown_system && prev_shutdown) {
        Send_History_Event("resume", "");
    }
    
    prev_shutdown = shutdown_system;
}

void Send_History_Event(const char* event_type, const char* extra_data) {
    char buffer[200];
    sprintf(buffer, "{\"event\":\"%s\",\"time\":%lu%s}\r\n", 
            event_type, system_millis, extra_data);
    
    while(!TXSTAbits.TRMT);
    for(int i = 0; buffer[i]; i++) {
        TXREG = buffer[i];
        while(!TXSTAbits.TRMT);
    }
}

void Read_Sensors(void) {
    // Leer temperatura
    unsigned int temp_adc = ADC_Read(LM35_CHANNEL);
    float raw_temp = (temp_adc * 5.0 / 1024.0) / 0.01;
    temperature = Apply_Filter(raw_temp, temp_samples);
    
    // Leer sensor de llama
    unsigned int flame_adc = ADC_Read(FLAME_CHANNEL);
    float flame_voltage = (flame_adc * 5.0) / 1024.0;
    flame_intensity = (flame_base_voltage - flame_voltage) * 100.0 / flame_base_voltage;
    
    if(flame_intensity < 0) flame_intensity = 0;
    if(flame_intensity > 100) flame_intensity = 100;
    
    // Detección con histéresis
    if(!flame_detected && flame_intensity >= FLAME_DETECTION_THRESHOLD) {
        flame_detected = true;
    } else if(flame_detected && flame_intensity <= (FLAME_DETECTION_THRESHOLD - FLAME_HYSTERESIS)) {
        flame_detected = false;
    }
    
    // Leer MQ2
    unsigned int mq2_adc = ADC_Read(MQ2_CHANNEL);
    float mq2_voltage = (mq2_adc * 5.0) / 1024.0;
    float Rs = (5.0 - mq2_voltage) / mq2_voltage;
    
    // Compensación por temperatura
    float temp_factor = 1.0 + 0.02 * (temperature - 25.0);
    Rs /= temp_factor;
    
    float rs_ro_ratio = Rs / MQ2_Ro;
    float raw_co = 0.0;
    
    if(rs_ro_ratio > 0.1) {
        raw_co = 15.0 * pow(rs_ro_ratio, -1.5);
    }
    
    if(raw_co < 0) raw_co = 0;
    if(raw_co > 1000) raw_co = 1000;
    
    co_ppm = Apply_Filter(raw_co, co_samples);
    
    // Calcular flujo
    static unsigned int last_pulse = 0;
    static unsigned long last_flow_time = 0;
    
    if(system_millis - last_flow_time >= 1000) {
        unsigned int pulses = pulse_count - last_pulse;
        flow_rate = (pulses / (float)FLOW_PULSES_PER_LITER) * 60.0;
        total_flow += flow_rate / 60.0;
        
        last_pulse = pulse_count;
        last_flow_time = system_millis;
    }
}

float Apply_Filter(float new_value, float* samples) {
    samples[sample_index] = new_value;
    
    float sum = 0;
    for(int i = 0; i < FILTER_SIZE; i++) {
        sum += samples[i];
    }
    return sum / FILTER_SIZE;
}

void Update_Actuators(void) {
    // Lógica de alarma simplificada
    fire_alarm = flame_detected || 
                 (temperature >= TEMP_ALARM_THRESHOLD) || 
                 (co_ppm >= CO_ALARM_THRESHOLD);
    
    if(fire_alarm) {
        PUMP_PIN = 1;
        pump_active = true;
        
        // Parpadear alarma
        static unsigned long last_blink = 0;
        if(system_millis - last_blink >= 500) {
            ALARM_PIN = !ALARM_PIN;
            last_blink = system_millis;
        }
        alarm_active = true;
    } else {
        PUMP_PIN = 0;
        pump_active = false;
        ALARM_PIN = 1;
        alarm_active = false;
    }
}

void Send_Data(void) {
    char buffer[150];
    sprintf(buffer, 
        "{\"t\":%.1f,\"fd\":%d,\"fi\":%.1f,\"co\":%.1f,\"fr\":%.2f,\"tf\":%.2f,\"p\":%d,\"a\":%d,\"cmd\":{\"test\":%d,\"shutdown\":%d}}\r\n",
        temperature, flame_detected, flame_intensity, co_ppm, flow_rate, total_flow,
        pump_active, alarm_active, trigger_test, shutdown_system);
    
    while(!TXSTAbits.TRMT);
    for(int i = 0; buffer[i]; i++) {
        TXREG = buffer[i];
        while(!TXSTAbits.TRMT);
    }
}

void ADC_Init(void) {
    TRISA0 = TRISA1 = TRISA2 = 1;
    ADCON0bits.ADON = 1;
    ADCON1 = 0x0C;
    ADCON2 = 0xBE;
}

unsigned int ADC_Read(unsigned char channel) {
    ADCON0bits.CHS = channel;
    __delay_us(30);
    ADCON0bits.GO = 1;
    while(ADCON0bits.GO);
    return (ADRESH << 8) | ADRESL;
}

void Interrupt_Init(void) {
    TRISBbits.TRISB1 = 1;
    INTCON2bits.INTEDG1 = 0;
    INTCON3bits.INT1IE = 1;
    INTCON3bits.INT1IP = 1;
    RCONbits.IPEN = 1;
    INTCONbits.GIEH = 1;
    INTCONbits.GIEL = 1;
}

void UART_Init(void) {
    TRISC6 = 0;
    TRISC7 = 1;
    SPBRG = 51;
    TXSTAbits.BRGH = 1;
    TXSTAbits.SYNC = 0;
    TXSTAbits.TXEN = 1;
    RCSTAbits.SPEN = 1;
    RCSTAbits.CREN = 1;
}