/*
 * Sistema anti-incendios con historial de eventos - PIC18F4550
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

// Umbrales
#define CO_ALARM_THRESHOLD_HIGH 55.0
#define CO_ALARM_THRESHOLD_LOW  45.0
#define TEMP_ALARM_THRESHOLD_HIGH 42.0
#define TEMP_ALARM_THRESHOLD_LOW  38.0
#define FLAME_DETECTION_THRESHOLD 18.0
#define FLAME_RELEASE_THRESHOLD   12.0

// Sensor de flujo
#define FLOW_PULSES_PER_LITER 450

// Variables globales
float temperature = 0.0;
float flame_intensity = 0.0;
float flame_base_voltage = 0.0;
bool flame_calibrated = false;
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

// Filtro
#define FILTER_SAMPLES 7
float temp_filter[FILTER_SAMPLES] = {0};
float co_filter[FILTER_SAMPLES] = {0};
unsigned char filter_index = 0;

// Tiempos
#define MQ2_RECALIBRATION_INTERVAL 300000
#define FLAME_RECALIBRATION_INTERVAL 600000
unsigned long calibrationCounter = 0;

// Comandos
bool shutdown_system = false;
bool trigger_test = false;
unsigned long test_start_time = 0;
unsigned long test_last_toggle = 0;
const unsigned long TEST_DURATION = 10000;
const unsigned long TEST_BLINK_INTERVAL = 500;

// Tiempo del sistema
unsigned long system_millis = 0;

// ===== HISTORIAL DE EVENTOS ===== //
#define FLOW_RESET_DELAY 5000

// Estados de eventos
typedef enum {
    EVENT_NONE,
    EVENT_FIRE,
    EVENT_TEST,
    EVENT_SHUTDOWN,
    EVENT_SYSTEM_START
} EventType;

// Estructura de evento
typedef struct {
    EventType type;
    unsigned long start_time;
    float start_temperature;
    float start_flame_intensity;
    float start_co_ppm;
    float start_total_flow;
    float water_used;
} EventHistory;

// Variables de eventos
EventHistory current_event = {EVENT_NONE};
bool event_active = false;
unsigned long last_flow_reset_time = 0;
bool system_started = false;
// ================================ //

// Prototipos
void ADC_Init(void);
unsigned int ADC_Read(unsigned char channel);
unsigned int ADC_Read_Average(unsigned char channel, unsigned char samples);
void Calibrate_Flame_Sensor(void);
void Calibrate_MQ2(void);
void Read_LM35(void);
void Read_Flame_Sensor(void);
void Read_MQ2_Sensor(void);
void Calculate_Flow(void);
void Send_Sensor_Data(void);
void UART_Init(void);
void Update_Actuators(void);
void Interrupt_Init(void);
void Handle_Test_Command(void);
char UART_Read(void);
char UART_Data_Ready(void);
void Start_Event(EventType type);
void End_Event();
void Check_Flow_Reset();
void Send_Event_Data(EventType type);

// Interrupción
void __interrupt(high_priority) HighISR(void) {
    if (INTCON3bits.INT1IF) {
        pulse_count++;
        INTCON3bits.INT1IF = 0;
    }
}

void main(void) {
    OSCCON = 0x70;
    TRISB = 0x02;
    PORTB = 0x00;
    ALARM_PIN = 1;
    PUMP_PIN = 0;
    
    // Inicializar filtros
    for(int i = 0; i < FILTER_SAMPLES; i++) {
        temp_filter[i] = co_filter[i] = 25.0;
    }
    
    __delay_ms(2000);
    UART_Init();
    ADC_Init();
    Interrupt_Init();
    
    // Evento de inicio del sistema
    Send_Event_Data(EVENT_SYSTEM_START);
    system_started = true;
    
    Calibrate_Flame_Sensor();
    Calibrate_MQ2();
    
    unsigned int sensor_counter = 0;
    pulse_count = 0;
    
    while(1) {
        // Actualizar tiempo
        system_millis += 250;
        
        // Procesar comandos UART
        if(UART_Data_Ready()) {
            char cmd = UART_Read();
            switch(cmd) {
                case 'T': // Trigger test
                    if(!trigger_test) {
                        trigger_test = true;
                        test_start_time = system_millis;
                        test_last_toggle = system_millis;
                    }
                    break;
                case 'S': // Shutdown system
                    shutdown_system = true;
                    break;
                case 'R': // Resume system
                    shutdown_system = false;
                    break;
            }
        }
        
        // Manejar prueba
        if(trigger_test) {
            Handle_Test_Command();
        }
        
        // Leer sensores si no está apagado
        if(!shutdown_system) {
            Read_LM35();
            Read_Flame_Sensor();
            Read_MQ2_Sensor();
            Calculate_Flow();
            
            if(!trigger_test) {
                Update_Actuators();
            }
        }
        
        // Manejar eventos
        if (fire_alarm && !event_active) {
            Start_Event(EVENT_FIRE);
        }
        else if (trigger_test && !event_active) {
            Start_Event(EVENT_TEST);
        }
        else if (shutdown_system && !event_active) {
            Start_Event(EVENT_SHUTDOWN);
        }
        else if (!fire_alarm && !trigger_test && !shutdown_system && event_active) {
            End_Event();
        }
        
        // Resetear flujo
        Check_Flow_Reset();
        
        filter_index = (filter_index + 1) % FILTER_SAMPLES;
        
        if(sensor_counter >= 4) {
            Send_Sensor_Data();
            sensor_counter = 0;
            
            calibrationCounter += 1000;
            if(calibrationCounter >= MQ2_RECALIBRATION_INTERVAL) {
                Calibrate_MQ2();
            }
            if(calibrationCounter >= FLAME_RECALIBRATION_INTERVAL) {
                Calibrate_Flame_Sensor();
                calibrationCounter = 0;
            }
        }
        
        sensor_counter++;
        __delay_ms(250);
    }
}

// ===== FUNCIONES DE EVENTOS ===== //
void Send_Event_Data(EventType type) {
    const char* event_type_str = "";
    switch(type) {
        case EVENT_FIRE: event_type_str = "fire"; break;
        case EVENT_TEST: event_type_str = "test"; break;
        case EVENT_SHUTDOWN: event_type_str = "shutdown"; break;
        case EVENT_SYSTEM_START: event_type_str = "system_start"; break;
        default: event_type_str = "unknown";
    }
    
    char event_buffer[100];
    sprintf(event_buffer, "{\"event\":\"%s\",\"time\":%lu}\r\n",
            event_type_str, system_millis);
    
    // Enviar por UART
    while(!TXSTAbits.TRMT);
    for(int i = 0; event_buffer[i]; i++) {
        TXREG = event_buffer[i];
        while(!TXSTAbits.TRMT);
    }
}

void Start_Event(EventType type) {
    current_event.type = type;
    current_event.start_time = system_millis;
    current_event.start_temperature = temperature;
    current_event.start_flame_intensity = flame_intensity;
    current_event.start_co_ppm = co_ppm;
    current_event.start_total_flow = total_flow;
    current_event.water_used = 0.0;
    event_active = true;
    
    // Enviar evento de inicio inmediatamente
    Send_Event_Data(type);
}

void End_Event() {
    if (!event_active) return;
    
    // Calcular agua utilizada
    current_event.water_used = total_flow - current_event.start_total_flow;
    
    // Enviar datos finales del evento
    char event_buffer[200];
    const char* event_type_str = "";
    switch(current_event.type) {
        case EVENT_FIRE: event_type_str = "fire_end"; break;
        case EVENT_TEST: event_type_str = "test_end"; break;
        case EVENT_SHUTDOWN: event_type_str = "shutdown_end"; break;
        default: event_type_str = "unknown";
    }
    
    sprintf(event_buffer, 
        "{\"event\":\"%s\",\"start_time\":%lu,\"t0\":%.1f,\"fi0\":%.1f,\"co0\":%.1f,\"water\":%.2f}\r\n",
        event_type_str,
        current_event.start_time,
        current_event.start_temperature,
        current_event.start_flame_intensity,
        current_event.start_co_ppm,
        current_event.water_used);
    
    // Enviar por UART
    while(!TXSTAbits.TRMT);
    for(int i = 0; event_buffer[i]; i++) {
        TXREG = event_buffer[i];
        while(!TXSTAbits.TRMT);
    }
    
    // Resetear evento
    event_active = false;
    current_event.type = EVENT_NONE;
    
    // Programar reset de flujo
    last_flow_reset_time = system_millis;
}

void Check_Flow_Reset() {
    if (last_flow_reset_time > 0 && 
        (system_millis - last_flow_reset_time) >= FLOW_RESET_DELAY) {
        total_flow = 0.0;
        pulse_count = 0;
        last_flow_reset_time = 0;
    }
}
// ================================ //

void Handle_Test_Command(void) {
    // Mantener prueba activa
    trigger_test = true;
    
    // Actuar bombas y alarmas
    PUMP_PIN = 1;
    pump_active = true;
    
    // Parpadear alarma
    if(system_millis - test_last_toggle >= TEST_BLINK_INTERVAL) {
        ALARM_PIN = !ALARM_PIN;
        test_last_toggle = system_millis;
    }
    alarm_active = true;
    
    // Finalizar prueba después de 10 segundos
    if((system_millis - test_start_time) >= TEST_DURATION) {
        trigger_test = false;
        PUMP_PIN = 0;
        ALARM_PIN = 1;
        pump_active = false;
        alarm_active = false;
    }
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

unsigned int ADC_Read_Average(unsigned char channel, unsigned char samples) {
    unsigned long sum = 0;
    for(unsigned char i = 0; i < samples; i++) {
        sum += ADC_Read(channel);
        __delay_us(25);
    }
    return (unsigned int)(sum / samples);
}

void Calibrate_Flame_Sensor() {
    float sum = 0;
    for(unsigned char i = 0; i < 20; i++) {
        unsigned int adc_value = ADC_Read(FLAME_CHANNEL);
        sum += (adc_value * 5.0) / 1024.0;
        __delay_ms(150);
    }
    flame_base_voltage = sum / 20.0;
    flame_calibrated = true;
    flame_detected = false;
    flame_intensity = 0.0;
}

void Calibrate_MQ2() {
    float sum = 0;
    for(unsigned char i = 0; i < 70; i++) {
        unsigned int adc_value = ADC_Read(MQ2_CHANNEL);
        float voltage = (adc_value * 5.0) / 1024.0;
        float Rs = (5.0 - voltage) / voltage;
        sum += Rs;
        __delay_ms(200);
    }
    MQ2_Ro = sum / 70.0;
}

void Read_LM35(void) {
    unsigned int adc_value = ADC_Read_Average(LM35_CHANNEL, 10);
    float raw_temp = (adc_value * 5.0 / 1024.0) / 0.01;
    
    temp_filter[filter_index] = raw_temp;
    temperature = 0;
    for(int i = 0; i < FILTER_SAMPLES; i++) {
        temperature += temp_filter[i];
    }
    temperature /= FILTER_SAMPLES;
}

void Read_Flame_Sensor(void) {
    if(!flame_calibrated) {
        Calibrate_Flame_Sensor();
        return;
    }
    
    unsigned int adc_value = ADC_Read_Average(FLAME_CHANNEL, 10);
    float voltage = (adc_value * 5.0) / 1024.0;
    float intensity = (flame_base_voltage - voltage) * 100.0 / flame_base_voltage;
    
    if(intensity < 0) intensity = 0;
    if(intensity > 100) intensity = 100;
    
    static bool last_detected = false;
    
    if (!last_detected && intensity >= FLAME_DETECTION_THRESHOLD) {
        flame_detected = true;
        last_detected = true;
    } 
    else if (last_detected && intensity <= FLAME_RELEASE_THRESHOLD) {
        flame_detected = false;
        last_detected = false;
    }
    
    flame_intensity = flame_detected ? intensity : 0.0;
}

void Read_MQ2_Sensor(void) {
    static bool calibrated = false;
    if(!calibrated) {
        Calibrate_MQ2();
        calibrated = true;
    }
    
    unsigned int adc_value = ADC_Read_Average(MQ2_CHANNEL, 10);
    float voltage = (adc_value * 5.0) / 1024.0;
    float Rs = (5.0 - voltage) / voltage;
    
    float temp_factor = 1.0 + 0.025 * (temperature - 25.0);
    Rs /= temp_factor;
    
    float rs_ro_ratio = Rs / MQ2_Ro;
    float ppm = 0.0;
    
    if(rs_ro_ratio > 0.15) {
        ppm = 12.0 * pow(rs_ro_ratio, -1.8);
    }
    if(rs_ro_ratio > 0.05 && rs_ro_ratio <= 0.15) {
        ppm = 80.0 * pow(rs_ro_ratio, -3.2);
    }
    
    ppm = ppm < 0 ? 0 : (ppm > 10000 ? 10000 : ppm);
    
    co_filter[filter_index] = ppm;
    co_ppm = 0;
    for(int i = 0; i < FILTER_SAMPLES; i++) {
        co_ppm += co_filter[i];
    }
    co_ppm /= FILTER_SAMPLES;
}

void Calculate_Flow(void) {
    static unsigned int last_pulse_count = 0;
    static unsigned long last_calc_time = 0;
    
    // Calcular cada 1 segundo
    if(system_millis - last_calc_time >= 1000) {
        unsigned int pulses = pulse_count - last_pulse_count;
        flow_rate = (pulses / (float)FLOW_PULSES_PER_LITER) * 60.0;
        total_flow += flow_rate / 60.0;
        
        last_pulse_count = pulse_count;
        last_calc_time = system_millis;
    }
}

void Update_Actuators(void) {
    prev_fire_alarm = fire_alarm;
    
    bool temp_alarm = temperature >= TEMP_ALARM_THRESHOLD_HIGH;
    bool co_alarm = co_ppm >= CO_ALARM_THRESHOLD_HIGH;
    
    fire_alarm = flame_detected || temp_alarm || co_alarm;
    
    if (!fire_alarm && 
        (temperature > TEMP_ALARM_THRESHOLD_LOW || 
         co_ppm > CO_ALARM_THRESHOLD_LOW)) {
        fire_alarm = true;
    }

    if(fire_alarm) {
        PUMP_PIN = 1;
        pump_active = true;
        
        // Parpadear alarma (cada 500ms)
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

void Send_Sensor_Data(void) {
    char buffer[200];
    sprintf(buffer, "{\"t\":%.1f,\"fd\":%d,\"fi\":%.1f,\"co\":%.1f,\"fr\":%.2f,\"tf\":%.2f,\"p\":%d,\"a\":%d,\"cmd\":{\"test\":%d,\"shutdown\":%d}}\r\n",
            temperature, 
            flame_detected,
            flame_intensity,
            co_ppm,
            flow_rate,
            total_flow,
            pump_active,
            alarm_active,
            trigger_test,
            shutdown_system);
    while(!TXSTAbits.TRMT);
    for(int i = 0; buffer[i]; i++) {
        TXREG = buffer[i];
        while(!TXSTAbits.TRMT);
    }
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

char UART_Read(void) {
    if(PIR1bits.RCIF) {
        return RCREG;
    }
    return 0;
}

char UART_Data_Ready(void) {
    return PIR1bits.RCIF;
}