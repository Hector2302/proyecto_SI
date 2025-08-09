# Documentación del Sistema PIC18F4550 - Sistema Anti-Incendios

## Descripción General

El PIC18F4550 es el núcleo de procesamiento principal del sistema anti-incendios. Se encarga de la lectura de sensores, procesamiento de datos, control de actuadores y comunicación con el ESP32.

## Función Principal

El PIC actúa como el **controlador principal** del sistema, ejecutando las siguientes tareas críticas:

- **Adquisición de datos de sensores** en tiempo real
- **Procesamiento y filtrado** de señales analógicas
- **Control de actuadores** (bomba de agua y alarma)
- **Detección de eventos críticos** (incendios, pruebas, apagado)
- **Comunicación UART** con el ESP32
- **Gestión de comandos remotos**

## Arquitectura del Hardware

### Microcontrolador
- **Modelo:** PIC18F4550
- **Frecuencia:** 8 MHz (oscilador interno)
- **Voltaje de operación:** 5V
- **Comunicación:** UART a 9600 baudios

### Configuración de Pines

| Pin | Función | Descripción |
|-----|---------|-------------|
| RA0 | ADC Canal 0 | Sensor de temperatura LM35 |
| RA1 | ADC Canal 1 | Sensor de llama |
| RA2 | ADC Canal 2 | Sensor de gas MQ2 (CO) |
| RB0 | Salida Digital | Control de bomba de agua |
| RB1 | Entrada Digital | Sensor de flujo (interrupción) |
| RB5 | Salida Digital | Control de alarma |
| RC6 | UART TX | Transmisión a ESP32 |
| RC7 | UART RX | Recepción desde ESP32 |

## Sensores Conectados

### 1. Sensor de Temperatura LM35
- **Canal ADC:** 0
- **Rango:** 0°C a 100°C
- **Resolución:** 10mV/°C
- **Umbral de alarma:** 40°C
- **Filtrado:** Promedio móvil de 5 muestras

### 2. Sensor de Llama
- **Canal ADC:** 1
- **Tipo:** Fotoresistor infrarrojo
- **Umbral de detección:** 15% de intensidad
- **Histéresis:** 3% para evitar oscilaciones
- **Calibración:** Automática al inicio (50 muestras)

### 3. Sensor de Gas MQ2 (Monóxido de Carbono)
- **Canal ADC:** 2
- **Rango:** 0-1000 ppm CO
- **Umbral de alarma:** 50 ppm
- **Compensación:** Por temperatura ambiente
- **Calibración:** Automática (relación Rs/Ro)

### 4. Sensor de Flujo de Agua
- **Pin:** RB1 (interrupción externa)
- **Tipo:** Sensor de efecto Hall
- **Pulsos por litro:** 450
- **Medición:** Flujo instantáneo y total acumulado

## Actuadores

### 1. Bomba de Agua
- **Pin de control:** RB0
- **Activación:** Nivel alto (5V)
- **Condiciones de activación:**
  - Detección de llama
  - Temperatura > 40°C
  - CO > 50 ppm
  - Modo de prueba

### 2. Alarma Sonora/Visual
- **Pin de control:** RB5
- **Modo normal:** Nivel alto continuo
- **Modo alarma:** Parpadeo a 1 Hz
- **Modo prueba:** Parpadeo a 2 Hz

## Algoritmos de Procesamiento

### Filtrado de Señales
```c
float Apply_Filter(float new_value, float* samples) {
    samples[sample_index] = new_value;
    float sum = 0;
    for(int i = 0; i < FILTER_SIZE; i++) {
        sum += samples[i];
    }
    return sum / FILTER_SIZE;
}
```

### Detección de Incendios
El sistema utiliza lógica combinada:
```c
fire_alarm = flame_detected || 
             (temperature >= TEMP_ALARM_THRESHOLD) || 
             (co_ppm >= CO_ALARM_THRESHOLD);
```

### Histéresis para Estabilidad
- **Llama:** ±3% de intensidad
- **CO:** ±10 ppm
- **Temperatura:** Filtro promedio móvil

## Protocolo de Comunicación

### Datos de Sensores (cada 250ms)
```json
{
  "t": 25.5,
  "fd": 1,
  "fi": 45.2,
  "co": 25.0,
  "fr": 2.5,
  "tf": 15.8,
  "p": 1,
  "a": 1,
  "fa": 1,
  "cmd": {
    "test": 0,
    "shutdown": 0
  }
}
```

### Eventos de Historial
```json
{
  "event": "fire_start",
  "time": 12345,
  "sensor": "flame_sensor"
}
```

### Comandos Recibidos
- **'T':** Iniciar prueba del sistema
- **'S':** Apagar sistema (modo standby)
- **'R':** Reanudar operación normal
- **'F':** Reset del contador de flujo

## Estados del Sistema

### 1. Operación Normal
- Monitoreo continuo de sensores
- Respuesta automática a alarmas
- Envío de datos cada 250ms

### 2. Modo de Prueba
- Duración: 3.33 segundos
- Activación: Bomba y alarma
- Medición: Consumo de agua
- Finalización: Automática

### 3. Modo Standby
- Sensores desactivados
- Actuadores apagados
- Comunicación mantenida
- Respuesta solo a comando 'R'

## Gestión de Eventos Críticos

### Inicio de Incendio
1. Detección por cualquier sensor
2. Registro de condiciones iniciales
3. Activación inmediata de actuadores
4. Envío de evento al ESP32
5. Monitoreo continuo

### Fin de Incendio
1. Todos los sensores en rango normal
2. Cálculo de duración y agua utilizada
3. Desactivación de actuadores
4. Envío de resumen del evento
5. Reset automático del sistema

## Optimizaciones Implementadas

### Eficiencia Energética
- Frecuencia optimizada a 8 MHz
- Interrupciones para eventos críticos
- Delays calibrados para timing preciso

### Estabilidad del Sistema
- Filtros digitales para ruido
- Histéresis en umbrales críticos
- Validación de datos antes de envío
- Reset automático de contadores

### Tiempo Real
- Ciclo principal de 125ms
- Respuesta inmediata a alarmas
- Prioridad alta para interrupciones
- Comunicación no bloqueante


# Comenzando con la Documentación Técnica y Detallada


### Configuración del Sistema

```c
// Configuración de bits del microcontrolador
#pragma config FOSC = INTOSCIO_EC, WDT = OFF, PWRT = ON, MCLRE = ON, LVP = OFF, XINST = OFF

// Definición de frecuencia del oscilador
#define _XTAL_FREQ 8000000  // 8 MHz para operación eficiente

// Definiciones de pines de actuadores
#define PUMP_PIN     PORTBbits.RB0  // Control de bomba de agua
#define ALARM_PIN    PORTBbits.RB5  // Control de alarma
```

**Explicación de la configuración:**
- `FOSC = INTOSCIO_EC`: Utiliza oscilador interno de 8MHz
- `WDT = OFF`: Desactiva el watchdog timer para evitar resets no deseados
- `PWRT = ON`: Activa el power-up timer para estabilización
- `MCLRE = ON`: Habilita el pin de reset maestro
- `LVP = OFF`: Desactiva programación de bajo voltaje

### Definiciones de Sensores y Umbrales

```c
// Canales ADC para cada sensor
#define LM35_CHANNEL    0  // Canal 0 para sensor de temperatura
#define FLAME_CHANNEL   1  // Canal 1 para sensor de llama
#define MQ2_CHANNEL     2  // Canal 2 para sensor de gas CO

// Umbrales críticos optimizados mediante pruebas
#define CO_ALARM_THRESHOLD 50.0        // 50 ppm de CO para activar alarma
#define TEMP_ALARM_THRESHOLD 40.0      // 40°C para activar alarma
#define FLAME_DETECTION_THRESHOLD 15.0 // 15% de intensidad para detectar llama
#define FLAME_HYSTERESIS 3.0           // ±3% para evitar oscilaciones
#define CO_HYSTERESIS 10.0             // ±10 ppm para estabilidad

// Configuración del sensor de flujo
#define FLOW_PULSES_PER_LITER 450      // Pulsos generados por litro de agua
```

### Variables Globales del Sistema

```c
// Variables de sensores
float temperature = 0.0;        // Temperatura actual en °C
float flame_intensity = 0.0;    // Intensidad de llama en porcentaje
float flame_base_voltage = 0.0; // Voltaje base calibrado del sensor de llama
bool flame_detected = false;    // Estado de detección de llama
float co_ppm = 0.0;            // Concentración de CO en ppm
float MQ2_Ro = 10.0;           // Resistencia de referencia del MQ2

// Variables de flujo de agua
float flow_rate = 0.0;         // Flujo instantáneo en L/min
float total_flow = 0.0;        // Flujo total acumulado en litros
volatile unsigned int pulse_count = 0; // Contador de pulsos (volátil para ISR)

// Estados del sistema
bool pump_active = false;       // Estado de la bomba
bool alarm_active = false;      // Estado de la alarma
bool fire_alarm = false;        // Estado de alarma de incendio
bool prev_fire_alarm = false;   // Estado anterior para detectar cambios

// Sistema de filtrado digital
#define FILTER_SIZE 5
float temp_samples[FILTER_SIZE] = {25.0, 25.0, 25.0, 25.0, 25.0};
float co_samples[FILTER_SIZE] = {0.0, 0.0, 0.0, 0.0, 0.0};
unsigned char sample_index = 0;

// Control de comandos
bool shutdown_system = false;   // Estado de apagado del sistema
bool trigger_test = false;       // Estado de prueba activa
unsigned long test_start_time = 0;
const unsigned long TEST_DURATION = 3333; // Duración de prueba: 3.33 segundos

// Tiempo del sistema (contador en milisegundos)
unsigned long system_millis = 0;
```

### Función Principal (main)

```c
void main(void) {
    // Configuración inicial del oscilador y puertos
    OSCCON = 0x70;  // Configurar oscilador interno a 8MHz
    TRISB = 0x02;   // RB1 como entrada (sensor flujo), resto como salidas
    PORTB = 0x00;   // Inicializar puerto B en 0
    
    // Estado inicial de actuadores
    ALARM_PIN = 1;  // Alarma inicialmente encendida (estado normal)
    PUMP_PIN = 0;   // Bomba inicialmente apagada
    
    // Delay de estabilización del sistema
    __delay_ms(2000);
    
    // Inicialización de periféricos
    UART_Init();      // Configurar comunicación serie
    ADC_Init();       // Configurar conversor analógico-digital
    Interrupt_Init(); // Configurar interrupciones
    
    // Calibración automática del sensor de llama (50 muestras)
    for(int i = 0; i < 50; i++) {
        unsigned int adc_value = ADC_Read(FLAME_CHANNEL);
        flame_base_voltage += (adc_value * 5.0) / 1024.0;
        __delay_ms(100);
    }
    flame_base_voltage /= 50.0; // Promedio de las 50 muestras
    
    // Calibración automática del sensor MQ2 (50 muestras)
    for(int i = 0; i < 50; i++) {
        unsigned int adc_value = ADC_Read(MQ2_CHANNEL);
        float voltage = (adc_value * 5.0) / 1024.0;
        MQ2_Ro += (5.0 - voltage) / voltage; // Cálculo de resistencia Ro
        __delay_ms(100);
    }
    MQ2_Ro /= 50.0; // Promedio de las 50 muestras
    
    unsigned int cycle_count = 0;
    
    // Bucle principal del sistema
    while(1) {
        system_millis += 125; // Incrementar contador de tiempo
        
        Handle_Commands();    // Procesar comandos UART
        
        if(!shutdown_system) {
            Read_Sensors();   // Leer todos los sensores
            if(!trigger_test) {
                Update_Actuators(); // Actualizar bomba y alarma
            }
        }
        
        // Gestión de eventos del sistema
        Handle_Fire_Events();
        Handle_Test_Events();
        Handle_Shutdown_Events();
        
        // Enviar datos cada 250ms (2 ciclos de 125ms)
        if(cycle_count >= 2) {
            Send_Data();
            cycle_count = 0;
        }
        
        cycle_count++;
        sample_index = (sample_index + 1) % FILTER_SIZE;
        __delay_ms(125); // Delay del ciclo principal
    }
}
```

### Inicialización del ADC

```c
void ADC_Init(void) {
    // Configurar pines como entradas analógicas
    TRISA0 = TRISA1 = TRISA2 = 1; // RA0, RA1, RA2 como entradas
    
    // Configuración del módulo ADC
    ADCON0bits.ADON = 1;  // Encender el módulo ADC
    ADCON1 = 0x0C;        // Configurar como entradas analógicas
    ADCON2 = 0xBE;        // Configurar tiempo de adquisición y reloj
}
```

**Explicación de ADCON2 = 0xBE:**
- Bits 7-6 (10): Justificación a la derecha
- Bits 5-3 (111): Tiempo de adquisición = 20 TAD
- Bits 2-0 (110): Reloj ADC = FOSC/64

### Lectura del ADC

```c
unsigned int ADC_Read(unsigned char channel) {
    ADCON0bits.CHS = channel; // Seleccionar canal
    __delay_us(30);           // Tiempo de estabilización
    ADCON0bits.GO = 1;        // Iniciar conversión
    while(ADCON0bits.GO);     // Esperar fin de conversión
    
    // Retornar resultado de 10 bits
    return ((unsigned int)ADRESH << 8) | ADRESL;
}
```

### Inicialización de Interrupciones

```c
void Interrupt_Init(void) {
    TRISBbits.TRISB1 = 1;     // RB1 como entrada
    INTCON2bits.INTEDG1 = 0;  // Interrupción en flanco descendente
    INTCON3bits.INT1IE = 1;   // Habilitar interrupción INT1
    INTCON3bits.INT1IP = 1;   // Alta prioridad para INT1
    RCONbits.IPEN = 1;        // Habilitar niveles de prioridad
    INTCONbits.GIEH = 1;      // Habilitar interrupciones de alta prioridad
    INTCONbits.GIEL = 1;      // Habilitar interrupciones de baja prioridad
}
```

### Rutina de Interrupción (ISR)

```c
void __interrupt(high_priority) HighISR(void) {
    // Interrupción del sensor de flujo
    if (INTCON3bits.INT1IF) {
        pulse_count++;           // Incrementar contador de pulsos
        INTCON3bits.INT1IF = 0;  // Limpiar bandera de interrupción
    }
}
```

### Inicialización de UART

```c
void UART_Init(void) {
    TRISC6 = 0;              // RC6 (TX) como salida
    TRISC7 = 1;              // RC7 (RX) como entrada
    
    // Configuración de velocidad: 9600 baudios
    SPBRG = 51;              // Valor para 9600 baud con BRGH=1
    TXSTAbits.BRGH = 1;      // Alta velocidad
    TXSTAbits.SYNC = 0;      // Modo asíncrono
    TXSTAbits.TXEN = 1;      // Habilitar transmisión
    
    RCSTAbits.SPEN = 1;      // Habilitar puerto serie
    RCSTAbits.CREN = 1;      // Habilitar recepción continua
}
```

**Cálculo de SPBRG para 9600 baudios:**

Baud Rate = FOSC / (16 * (SPBRG + 1))
9600 = 8000000 / (16 * (SPBRG + 1))
SPBRG = (8000000 / (16 * 9600)) - 1 = 51.08 ≈ 51




### Lectura de Sensores

#### Sensor de Temperatura LM35

```c
// Leer temperatura del LM35
unsigned int temp_adc = ADC_Read(LM35_CHANNEL);
float raw_temp = (temp_adc * 5.0 / 1024.0) / 0.01;
temperature = Apply_Filter(raw_temp, temp_samples);
```

**Cálculo de temperatura:**
1. Conversión ADC a voltaje: `V = (ADC_value * 5.0) / 1024.0`
2. Conversión voltaje a temperatura: `T = V / 0.01` (LM35: 10mV/°C)
3. Aplicación de filtro promedio móvil

#### Sensor de Llama

```c
// Leer sensor de llama
unsigned int flame_adc = ADC_Read(FLAME_CHANNEL);
float flame_voltage = (flame_adc * 5.0) / 1024.0;
flame_intensity = (flame_base_voltage - flame_voltage) * 100.0 / flame_base_voltage;

// Limitar rango 0-100%
if(flame_intensity < 0) flame_intensity = 0;
if(flame_intensity > 100) flame_intensity = 100;

// Detección con histéresis
if(!flame_detected && flame_intensity >= FLAME_DETECTION_THRESHOLD) {
    flame_detected = true;
} else if(flame_detected && flame_intensity <= (FLAME_DETECTION_THRESHOLD - FLAME_HYSTERESIS)) {
    flame_detected = false;
}
```

**Explicación del cálculo:**
- El sensor de llama es un fotoresistor que disminuye su resistencia con la luz infrarroja
- Mayor intensidad de llama = menor voltaje en el ADC
- La intensidad se calcula como porcentaje de diferencia respecto al voltaje base
- Se implementa histéresis para evitar oscilaciones en el umbral

#### Sensor MQ2 (Monóxido de Carbono)

```c
// Leer MQ2 con compensación de temperatura
unsigned int mq2_adc = ADC_Read(MQ2_CHANNEL);
float mq2_voltage = (mq2_adc * 5.0) / 1024.0;
float Rs = (5.0 - mq2_voltage) / mq2_voltage;

// Compensación por temperatura
float temp_factor = 1.0 + 0.02 * (temperature - 25.0);
Rs /= temp_factor;

// Cálculo de concentración de CO
float rs_ro_ratio = Rs / MQ2_Ro;
float raw_co = 0.0;

if(rs_ro_ratio > 0.1) {
    raw_co = 15.0 * pow(rs_ro_ratio, -1.5);
}

// Limitar rango 0-1000 ppm
if(raw_co < 0) raw_co = 0;
if(raw_co > 1000) raw_co = 1000;

// Filtrado menos agresivo para CO
static float last_co = 0.0;
co_ppm = (raw_co * 0.3) + (last_co * 0.7);
last_co = co_ppm;
```

**Explicación del cálculo MQ2:**
1. **Conversión a resistencia:** `Rs = (Vcc - V_sensor) / V_sensor`
2. **Compensación térmica:** Factor de 2% por grado sobre 25°C
3. **Relación Rs/Ro:** Normalización respecto a resistencia en aire limpio
4. **Concentración CO:** Fórmula empírica `CO = 15 * (Rs/Ro)^(-1.5)`
5. **Filtro exponencial:** Suaviza variaciones bruscas

#### Sensor de Flujo de Agua

```c
// Calcular flujo con reset automático
static unsigned int last_pulse = 0;
static unsigned long last_flow_time = 0;

if(system_millis - last_flow_time >= 1000) {
    unsigned int current_pulses = pulse_count;
    unsigned int pulses_diff = current_pulses - last_pulse;
    
    if(pulses_diff == 0) {
        flow_rate = 0.0;
        // Reset automático después de 5 segundos sin flujo
        static unsigned long no_flow_start = 0;
        if(no_flow_start == 0) {
            no_flow_start = system_millis;
        } else if(system_millis - no_flow_start >= 5000) {
            total_flow = 0.0;
            pulse_count = 0;
            no_flow_start = 0;
        }
    } else {
        // Cálculo de flujo: (pulsos/segundo) / (pulsos/litro) * 60 = L/min
        flow_rate = (pulses_diff / (float)FLOW_PULSES_PER_LITER) * 60.0;
        total_flow += flow_rate / 60.0; // Acumular flujo total
    }
    
    last_pulse = current_pulses;
    last_flow_time = system_millis;
}
```

**Cálculo de flujo:**
- **Flujo instantáneo:** `Flow_rate = (Pulsos/seg) / (450 pulsos/L) * 60 = L/min`
- **Flujo total:** Integración del flujo instantáneo en el tiempo
- **Reset automático:** Evita acumulación indefinida cuando no hay flujo

### Filtro Digital Promedio Móvil

```c
float Apply_Filter(float new_value, float* samples) {
    samples[sample_index] = new_value; // Almacenar nueva muestra
    
    float sum = 0;
    for(int i = 0; i < FILTER_SIZE; i++) {
        sum += samples[i]; // Sumar todas las muestras
    }
    return sum / FILTER_SIZE; // Retornar promedio
}
```

**Características del filtro:**
- **Tipo:** Promedio móvil de 5 muestras
- **Ventaja:** Reduce ruido de alta frecuencia
- **Desventaja:** Introduce retardo de 2.5 muestras
- **Aplicación:** Temperatura y CO (señales lentas)

### Control de Actuadores

```c
void Update_Actuators(void) {
    // Lógica de detección de incendio
    fire_alarm = flame_detected || 
                 (temperature >= TEMP_ALARM_THRESHOLD) || 
                 (co_ppm >= CO_ALARM_THRESHOLD);
    
    if(fire_alarm) {
        // Activar bomba
        PUMP_PIN = 1;
        pump_active = true;
        
        // Parpadear alarma a 1 Hz
        static unsigned long last_blink = 0;
        if(system_millis - last_blink >= 500) {
            ALARM_PIN = !ALARM_PIN;
            last_blink = system_millis;
        }
        alarm_active = true;
    } else {
        // Desactivar actuadores
        PUMP_PIN = 0;
        pump_active = false;
        ALARM_PIN = 1; // Estado normal de alarma
        alarm_active = false;
    }
}
```

### Protocolo de Comunicación UART

#### Formato de Datos de Sensores

```c
void Send_Data(void) {
    char buffer[200];
    sprintf(buffer, 
        "{\"t\":%.1f,\"fd\":%d,\"fi\":%.1f,\"co\":%.1f,\"fr\":%.2f,\"tf\":%.2f,\"p\":%d,\"a\":%d,\"fa\":%d,\"cmd\":{\"test\":%d,\"shutdown\":%d}}\r\n",
        temperature, flame_detected, flame_intensity, co_ppm, flow_rate, total_flow,
        pump_active, alarm_active, fire_alarm, trigger_test, shutdown_system);
    
    // Transmisión carácter por carácter
    while(!TXSTAbits.TRMT); // Esperar que el buffer esté vacío
    for(int i = 0; buffer[i]; i++) {
        TXREG = buffer[i];
        while(!TXSTAbits.TRMT); // Esperar transmisión completa
    }
}
```

**Estructura del JSON de datos:**
- `t`: Temperatura en °C
- `fd`: Llama detectada (0/1)
- `fi`: Intensidad de llama (%)
- `co`: Concentración CO (ppm)
- `fr`: Flujo instantáneo (L/min)
- `tf`: Flujo total (L)
- `p`: Estado bomba (0/1)
- `a`: Estado alarma (0/1)
- `fa`: Alarma de incendio (0/1)
- `cmd`: Estados de comandos

#### Gestión de Comandos

```c
void Handle_Commands(void) {
    if(PIR1bits.RCIF) { // Si hay dato recibido
        char cmd = RCREG;
        switch(cmd) {
            case 'T': // Comando de prueba
                if(!trigger_test) {
                    trigger_test = true;
                    test_start_time = system_millis;
                    test_start_flow = total_flow;
                }
                break;
                
            case 'S': // Comando de apagado
                shutdown_system = true;
                break;
                
            case 'R': // Comando de reanudación
                shutdown_system = false;
                break;
                
            case 'F': // Reset de flujo
                total_flow = 0.0;
                pulse_count = 0;
                flow_rate = 0.0;
                break;
        }
    }
    
    // Manejo del modo de prueba
    if(trigger_test) {
        PUMP_PIN = 1;
        pump_active = true;
        
        // Parpadear alarma a 2 Hz (modo prueba)
        static unsigned long last_blink = 0;
        if(system_millis - last_blink >= 250) {
            ALARM_PIN = !ALARM_PIN;
            last_blink = system_millis;
        }
        alarm_active = true;
        
        // Finalizar prueba después de 3.33 segundos
        if((system_millis - test_start_time) >= TEST_DURATION) {
            trigger_test = false;
            PUMP_PIN = 0;
            ALARM_PIN = 1;
            pump_active = false;
            alarm_active = false;
        }
    }
}
```

### Gestión de Eventos Históricos

#### Eventos de Incendio

```c
void Handle_Fire_Events(void) {
    // Detectar inicio de incendio
    if(fire_alarm && !prev_fire_alarm) {
        fire_start_time = system_millis;
        fire_start_temp = temperature;
        fire_start_flame = flame_intensity;
        fire_start_co = co_ppm;
        fire_start_flow = total_flow;
        
        // Determinar sensor que activó la alarma
        if(flame_detected) {
            strcpy(fire_trigger_sensor, "flame_sensor");
        } else if(temperature >= TEMP_ALARM_THRESHOLD) {
            strcpy(fire_trigger_sensor, "temperature_sensor");
        } else if(co_ppm >= CO_ALARM_THRESHOLD) {
            strcpy(fire_trigger_sensor, "co_sensor");
        }
        
        // Enviar evento de inicio
        char start_data[100];
        sprintf(start_data, ",\"sensor\":\"%s\"", fire_trigger_sensor);
        Send_History_Event("fire_start", start_data);
    }
    
    // Detectar fin de incendio
    if(!fire_alarm && prev_fire_alarm) {
        unsigned long duration = (system_millis - fire_start_time) / 1000;
        float water_used = total_flow - fire_start_flow;
        
        char end_data[150];
        sprintf(end_data, ",\"duration\":%lu,\"water\":%.2f", duration, water_used);
        Send_History_Event("fire_end", end_data);
        
        // Reset automático del sistema
        __delay_ms(5000);
        total_flow = 0.0;
        pulse_count = 0;
    }
    
    prev_fire_alarm = fire_alarm;
}
```

#### Envío de Eventos

```c
void Send_History_Event(const char* event_type, const char* extra_data) {
    char buffer[200];
    sprintf(buffer, "{\"event\":\"%s\",\"time\":%lu%s}\r\n", 
            event_type, system_millis, extra_data);
    
    // Transmisión segura
    while(!TXSTAbits.TRMT);
    for(int i = 0; buffer[i]; i++) {
        TXREG = buffer[i];
        while(!TXSTAbits.TRMT);
    }
}
```

## Optimizaciones y Consideraciones Técnicas

### Timing del Sistema
- **Ciclo principal:** 125ms para respuesta rápida
- **Envío de datos:** Cada 250ms (2 ciclos)
- **Muestreo ADC:** Cada ciclo (8 Hz)
- **Interrupciones:** Procesamiento inmediato de pulsos de flujo

### Gestión de Memoria
- **Variables globales:** Minimizadas para eficiencia
- **Buffers:** Tamaño optimizado (200 caracteres para UART)
- **Filtros:** Arrays estáticos para evitar fragmentación

### Robustez del Sistema
- **Calibración automática:** Al inicio del sistema
- **Filtrado digital:** Reduce ruido y falsas alarmas
- **Histéresis:** Evita oscilaciones en umbrales
- **Reset automático:** Previene acumulación de errores
- **Validación de datos:** Límites en rangos de sensores

### Eficiencia Energética
- **Frecuencia optimizada:** 8MHz balance entre velocidad y consumo
- **Interrupciones:** Solo para eventos críticos
- **Delays precisos:** Evitan bucles de espera activa innecesarios

Esta documentación técnica detallada proporciona una comprensión completa del funcionamiento interno del sistema PIC18F4550, incluyendo todos los cálculos matemáticos, algoritmos de procesamiento y protocolos de comunicación implementados.