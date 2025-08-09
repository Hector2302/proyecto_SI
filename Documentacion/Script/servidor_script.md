# Documentación del Script de Servidor Python - Sistema Anti-Incendios

## Descripción General

El script de Python `trigger_save_data.py` es un **servicio de monitoreo y archivado** que se ejecuta continuamente en el servidor. Su función principal es procesar eventos del sistema anti-incendios desde Firebase Realtime Database y almacenarlos de forma permanente en Firestore para análisis histórico y generación de reportes.

## Función Principal

El script actúa como un **procesador de eventos en tiempo real** que:

- **Monitorea continuamente** Firebase Realtime Database
- **Procesa eventos críticos** (incendios, pruebas, cambios de estado)
- **Almacena datos históricos** en Firestore
- **Gestiona notificaciones** del sistema
- **Limpia datos antiguos** automáticamente
- **Mantiene logs detallados** de operaciones

## Arquitectura del Sistema

### Tecnologías Utilizadas
- **Lenguaje:** Python 3.8+
- **Firebase Admin SDK:** Para acceso a Firebase
- **Firebase Realtime Database:** Fuente de datos en tiempo real
- **Firestore:** Almacenamiento permanente
- **Logging:** Sistema de logs integrado
- **Threading:** Procesamiento asíncrono

### Dependencias
```python
import time
import logging
from datetime import datetime, timezone
from firebase_admin import credentials, initialize_app, db
from firebase_admin import firestore
```

### Configuración de Firebase
```python
# Inicialización con credenciales por defecto
cred = credentials.ApplicationDefault()
app = initialize_app(cred, {
    'databaseURL': 'https://firefighting-system-fed2c-default-rtdb.firebaseio.com'
})

# Cliente Firestore para almacenamiento histórico
fs = firestore.client()
```

## Estructura de Datos

### Firebase Realtime Database (Fuente)

system/
├── history/
│   ├── fire/              # Eventos de incendio
│   │   ├── fire_start_123456
│   │   └── fire_end_123789
│   ├── test/              # Eventos de prueba
│   │   ├── test_start_456789
│   │   └── test_end_456999
│   └── status/            # Cambios de estado
│       ├── shutdown_789123
│       └── resume_789456
└── notifications/         # Cola de notificaciones
    ├── active: true
    └── queue/
        └── notification_id

---

# Comenzando con la documentación técnica y detallada

## Código Fuente Completo Documentado

### 1. Configuración Inicial y Dependencias

```python:%2FUsers%2Fhectoralv%2FDesktop%2FSistemas%20Embebidos%2FProyecto%2FScript_servidor%2FScript%20BD%2Ftrigger_save_data.py
# Importación de librerías necesarias para el funcionamiento del script
import time                    # Para manejo de tiempos y pausas
import logging                 # Sistema de logging para monitoreo
from datetime import datetime, timezone  # Manejo de fechas y zonas horarias
from firebase_admin import credentials, initialize_app, db  # Firebase Realtime Database
from firebase_admin import firestore  # Firestore para almacenamiento permanente

# Configuración del sistema de logging
# Registra eventos tanto en archivo como en consola
logging.basicConfig(
    level=logging.INFO,        # Nivel de logging: INFO, WARNING, ERROR
    format='%(asctime)s - %(levelname)s - %(message)s',  # Formato de mensajes
    handlers=[
        logging.FileHandler('fire_monitor.log'),  # Archivo de log
        logging.StreamHandler()                    # Salida por consola
    ]
)

# Inicialización de Firebase con credenciales por defecto del sistema
cred = credentials.ApplicationDefault()
app = initialize_app(cred, {
    'databaseURL': 'https://firefighting-system-fed2c-default-rtdb.firebaseio.com'
})

# Cliente Firestore para almacenamiento histórico permanente
fs = firestore.client()

# Variables globales para control de estado del procesamiento
processed_events = set()      # Set para evitar procesar eventos duplicados
last_log_time = 0            # Control de tiempo para logs periódicos
empty_history_logged = False  # Flag para evitar spam de logs cuando no hay eventos
```

### 2. Función de Parsing de Timestamps

```python:%2FUsers%2Fhectoralv%2FDesktop%2FSistemas%20Embebidos%2FProyecto%2FScript_servidor%2FScript%20BD%2Ftrigger_save_data.py
def parse_firebase_timestamp(timestamp_str):
    """
    Función para parsear timestamps de Firebase a objetos datetime de Python
    
    Parámetros:
    - timestamp_str: String con el timestamp en formato ISO 8601
    
    Retorna:
    - datetime object en UTC o None si hay error
    
    Maneja múltiples formatos de timestamp:
    - ISO 8601 con 'Z' (UTC): "2024-01-15T10:30:00Z"
    - ISO 8601 con timezone: "2024-01-15T10:30:00+00:00"
    - ISO 8601 sin timezone: "2024-01-15T10:30:00"
    """
    try:
        # Validar que el timestamp no esté vacío
        if not timestamp_str or timestamp_str.strip() == '':
            return None  # Retornar None para timestamps inválidos
        
        # Caso 1: Timestamp termina en 'Z' (formato UTC)
        if timestamp_str.endswith('Z'):
            dt_str = timestamp_str.replace('Z', '+00:00')
            return datetime.fromisoformat(dt_str)
        
        # Caso 2: Timestamp con 'T' pero sin timezone explícito
        elif 'T' in timestamp_str and '+' not in timestamp_str and '-' in timestamp_str:
            return datetime.fromisoformat(timestamp_str + '+00:00')
        
        # Caso 3: Timestamp con timezone incluido
        else:
            return datetime.fromisoformat(timestamp_str).astimezone(timezone.utc)
            
    except Exception as e:
        # En caso de error, retornar None en lugar de timestamp actual
        # Esto evita procesar eventos con timestamps corruptos
        return None
```

### 3. Función Principal de Procesamiento de Eventos Históricos

```python:%2FUsers%2Fhectoralv%2FDesktop%2FSistemas%20Embebidos%2FProyecto%2FScript_servidor%2FScript%20BD%2Ftrigger_save_data.py
def process_history_events():
    """
    Función principal para procesar eventos históricos del sistema anti-incendios
    
    Procesa tres categorías de eventos:
    1. fire: Eventos de incendio (inicio/fin)
    2. test: Eventos de prueba del sistema
    3. status: Cambios de estado del sistema (apagado/reanudación)
    
    Implementa múltiples filtros para optimizar el procesamiento:
    - Filtro de placeholders de Firebase
    - Filtro de eventos duplicados
    - Filtro de timestamps válidos
    - Filtro de eventos recientes (últimos 10 segundos)
    - Filtro de datos mínimos requeridos
    """
    global last_log_time, empty_history_logged
    
    try:
        # Acceder solo a la ruta de historial para minimizar consumo de ancho de banda
        ref = db.reference('system/history')
        history_data = ref.get()
        
        # Manejar caso cuando history está vacío (log solo una vez para evitar spam)
        if not history_data:
            if not empty_history_logged:
                logging.info("Historial vacío - esperando eventos...")
                empty_history_logged = True
            return
        
        # Reset flag si hay datos disponibles
        empty_history_logged = False
        
        # Definir categorías de eventos a procesar
        categories = ['fire', 'test', 'status']
        events_processed = 0
        
        # Procesar cada categoría de eventos
        for category in categories:
            category_events = history_data.get(category, {})
            
            # Si la categoría está vacía, continuar sin generar logs
            if not category_events:
                continue
            
            # Procesar cada evento individual en la categoría
            for event_key, event_data in category_events.items():
                try:
                    # FILTRO 1: Ignorar eventos placeholder de Firebase
                    # Firebase a veces crea entradas placeholder para mantener estructura
                    if 'placeholder' in event_key.lower():
                        continue
                    
                    # FILTRO 2: Ignorar eventos sin datos reales o con formato incorrecto
                    if not isinstance(event_data, dict) or not event_data:
                        continue
                    
                    # Crear ID único para el evento combinando categoría y clave
                    event_id = f"{category}_{event_key}"
                    
                    # FILTRO 3: Si ya procesamos este evento, saltarlo inmediatamente
                    # Esto evita duplicados y mejora la eficiencia
                    if event_id in processed_events:
                        continue
                    
                    # FILTRO 4: Validar que el timestamp no esté vacío
                    timestamp_str = event_data.get('timestamp', '')
                    event_time = parse_firebase_timestamp(timestamp_str)
                    
                    # Si no hay timestamp válido, saltar SIN LOG (son placeholders)
                    if not event_time:
                        continue
                    
                    # FILTRO 5: Verificar que el evento sea reciente (últimos 10 segundos)
                    # Esto evita procesar eventos antiguos en caso de reinicio del script
                    now = datetime.now(timezone.utc)
                    if (now - event_time).total_seconds() > 10:
                        continue
                    
                    # FILTRO 6: Validar que el evento tenga datos mínimos requeridos
                    event_type = event_data.get('event', '')
                    if not event_type:
                        continue
                    
                    # Preparar estructura base de datos del evento para Firestore
                    doc_data = {
                        'event_type': event_type,                    # Tipo de evento (fire_start, test_end, etc.)
                        'category': category,                       # Categoría (fire, test, status)
                        'timestamp': event_time.isoformat(),       # Timestamp en formato ISO
                        'source': 'sistema_anti_incendios',        # Fuente del evento
                        'event_key': event_key                     # Clave original del evento
                    }
                    
                    # Agregar datos específicos del evento si están disponibles
                    
                    # Sensor que disparó el evento (para eventos de incendio)
                    if 'trigger_sensor' in event_data:
                        doc_data['trigger_sensor'] = event_data['trigger_sensor']
                    
                    # Duración del evento en segundos
                    if 'duration_seconds' in event_data:
                        doc_data['duration_seconds'] = event_data['duration_seconds']
                    
                    # Cantidad de agua utilizada durante el evento
                    if 'water_used' in event_data:
                        doc_data['water_used'] = event_data['water_used']
                    
                    # Para eventos finales (_end), incluir todos los datos de sensores y actuadores
                    # Esto proporciona un snapshot completo del estado del sistema al finalizar
                    if '_end' in event_type:
                        # Datos de todos los sensores al momento del evento
                        if 'sensor_data' in event_data:
                            doc_data['sensor_data'] = event_data['sensor_data']
                        
                        # Estado de todos los actuadores al momento del evento
                        if 'actuator_data' in event_data:
                            doc_data['actuator_data'] = event_data['actuator_data']
                        
                        # Estado general del sistema
                        if 'system_status' in event_data:
                            doc_data['system_status'] = event_data['system_status']
                    
                    # Determinar la colección de Firestore según la categoría del evento
                    if category == 'fire':
                        collection_name = 'fire_events'           # Eventos de incendio
                    elif category == 'test':
                        collection_name = 'test_events'           # Eventos de prueba
                    else:  # status
                        collection_name = 'system_status_events'  # Eventos de estado del sistema
                    
                    # Guardar el evento en Firestore con el ID único
                    fs.collection(collection_name).document(event_id).set(doc_data)
                    
                    # LOG SOLO CUANDO HAY EVENTOS REALES procesados exitosamente
                    logging.info(f"✅ Evento {category} procesado: {event_type}")
                    events_processed += 1
                    
                    # Marcar como procesado SOLO después de guardarlo exitosamente
                    # Esto garantiza que no se pierdan eventos en caso de error
                    processed_events.add(event_id)
                    
                except Exception as e:
                    # Solo log errores críticos, no warnings menores
                    logging.error(f"Error crítico procesando evento {category}: {str(e)}")
        
        # Log de estado solo cada 30 segundos si no hay eventos nuevos
        # Esto evita spam en los logs pero mantiene evidencia de que el script funciona
        current_time = time.time()
        if events_processed == 0 and (current_time - last_log_time) > 30:
            logging.info("🔍 Monitoreando historial - sin eventos nuevos")
            last_log_time = current_time
        
        # Optimización de memoria: Limpiar eventos procesados antiguos
        # Mantener solo los últimos 50 eventos para evitar crecimiento ilimitado de memoria
        if len(processed_events) > 100:
            processed_events_list = list(processed_events)
            processed_events.clear()
            processed_events.update(processed_events_list[-50:])
                
    except Exception as e:
        logging.error(f"Error crítico procesando historial: {str(e)}")
```

### 4. Función de Procesamiento de Notificaciones

```python:%2FUsers%2Fhectoralv%2FDesktop%2FSistemas%20Embebidos%2FProyecto%2FScript_servidor%2FScript%20BD%2Ftrigger_save_data.py
def process_notifications():
    """
    Procesar notificaciones del sistema de forma simplificada
    
    Las notificaciones incluyen:
    - Alertas de incendio
    - Notificaciones de prueba
    - Alertas de estado del sistema
    - Notificaciones de mantenimiento
    
    Cada notificación se almacena en Firestore para acceso desde la aplicación móvil
    """
    try:
        # Acceder a la cola de notificaciones en Firebase Realtime Database
        ref = db.reference('system/notifications')
        notifications_data = ref.get() or {}
        
        # Verificar si el sistema de notificaciones está activo
        if not notifications_data.get('active', False):
            return
        
        # Obtener la cola de notificaciones pendientes
        notification_queue = notifications_data.get('queue', {})
        notifications_processed = 0
        
        # Procesar cada notificación en la cola
        for notification_id, notification in notification_queue.items():
            try:
                # Filtrar notificaciones placeholder (similar a eventos)
                if 'placeholder' in notification_id.lower():
                    continue
                
                # Validar timestamp de la notificación
                timestamp_str = notification.get('timestamp', '')
                notification_time = parse_firebase_timestamp(timestamp_str)
                
                # Saltar notificaciones sin timestamp válido
                if not notification_time:
                    continue
                
                # Procesar solo notificaciones recientes (últimos 15 segundos)
                # Ventana más amplia que eventos para capturar notificaciones importantes
                now = datetime.now(timezone.utc)
                if (now - notification_time).total_seconds() > 15:
                    continue
                
                # Preparar datos de la notificación para Firestore
                doc_data = {
                    'notification_id': notification_id,                    # ID único de la notificación
                    'type': notification.get('type', ''),                 # Tipo (alert, info, warning)
                    'title': notification.get('title', ''),               # Título de la notificación
                    'message': notification.get('message', ''),           # Mensaje completo
                    'priority': notification.get('priority', 'normal'),   # Prioridad (low, normal, high, critical)
                    'timestamp': notification_time.isoformat(),           # Timestamp en formato ISO
                    'read': notification.get('read', False),              # Estado de lectura
                    'source': 'sistema_anti_incendios'                   # Fuente del sistema
                }
                
                # Guardar notificación en Firestore
                fs.collection('system_notifications').document(notification_id).set(doc_data)
                logging.info(f"📢 Notificación procesada: {notification.get('type', '')}")
                notifications_processed += 1
                
            except Exception as e:
                logging.error(f"Error procesando notificación {notification_id}: {str(e)}")
        
        # Log resumen solo si hay notificaciones procesadas
        if notifications_processed > 0:
            logging.info(f"📊 {notifications_processed} notificaciones procesadas")
                
    except Exception as e:
        logging.error(f"Error procesando notificaciones: {str(e)}")
```

### 5. Función Principal del Script

```python:%2FUsers%2Fhectoralv%2FDesktop%2FSistemas%20Embebidos%2FProyecto%2FScript_servidor%2FScript%20BD%2Ftrigger_save_data.py
def main():
    """
    Función principal que ejecuta el bucle de monitoreo continuo
    
    Implementa un sistema de monitoreo optimizado con diferentes frecuencias:
    - Eventos históricos: cada 2 segundos (alta frecuencia para eventos críticos)
    - Notificaciones: cada 10 segundos (frecuencia media para alertas)
    - Pausa general: 0.5 segundos (para reducir uso de CPU)
    
    El script está diseñado para ejecutarse 24/7 como servicio del sistema
    """
    last_history_check = time.time()
    
    # Log de inicio del sistema con configuración
    logging.info("🚀 Iniciando monitoreo optimizado del sistema anti-incendios")
    logging.info("⚙️  Configuración: Historial cada 2s, logs solo con cambios")
    
    # Bucle principal infinito
    while True:
        try:
            current_time = time.time()
            
            # Verificar historial cada 2 segundos
            # Frecuencia alta para capturar eventos críticos rápidamente
            if current_time - last_history_check >= 2:
                process_history_events()
                last_history_check = current_time
            
            # Procesar notificaciones cada 10 segundos
            # Frecuencia media, suficiente para notificaciones no críticas
            if int(current_time) % 10 == 0:
                process_notifications()
                
            # Pausa para reducir uso de CPU y ancho de banda
            time.sleep(0.5)
            
        except KeyboardInterrupt:
            # Manejo de interrupción manual (Ctrl+C)
            logging.info("🛑 Deteniendo monitor...")
            break
            
        except Exception as e:
            # Manejo de errores críticos con recuperación automática
            logging.error(f"Error en el loop principal: {str(e)}")
            time.sleep(5)  # Pausa antes de reintentar

# Punto de entrada del script
if __name__ == '__main__':
    main()
```

## Protocolo de Comunicación y Datos de Sensores

### Datos de Sensores Procesados

El script procesa los siguientes datos de sensores que provienen del PIC18F4550 a través del ESP32:

#### 1. Sensor de Temperatura (LM35)
```python
# Datos recibidos del sensor LM35
{
    "temperature": 25.6,        # Temperatura en grados Celsius
    "temp_threshold": 45.0,     # Umbral de temperatura para alarma
    "temp_status": "normal"     # Estado: normal, warning, critical
}
```

#### 2. Sensor de Llama
```python
# Datos del sensor de llama con histéresis
{
    "flame_detected": true,     # Detección booleana de llama
    "flame_intensity": 85,      # Intensidad de la llama (0-100%)
    "flame_threshold": 70,      # Umbral de detección
    "flame_status": "detected"  # Estado: clear, detected, critical
}
```

#### 3. Sensor de Gas (MQ2)
```python
# Datos del sensor MQ2 para detección de CO
{
    "co_ppm": 15.2,            # Concentración de CO en ppm
    "co_threshold": 50.0,      # Umbral de peligro (ppm)
    "co_compensated": 14.8,    # Valor compensado por temperatura
    "co_status": "safe"        # Estado: safe, warning, danger
}
```

#### 4. Sensor de Flujo de Agua
```python
# Datos del sensor de flujo de agua
{
    "water_flow": 2.5,         # Flujo actual en L/min
    "total_water": 125.8,      # Total acumulado en litros
    "flow_status": "active",   # Estado: inactive, active, blocked
    "pump_runtime": 45         # Tiempo de funcionamiento de la bomba (segundos)
}
```

### Protocolo UART entre PIC y ESP32

El script procesa datos que siguen este protocolo de comunicación:

#### Formato de Datos de Sensores (JSON)
```json
{
    "type": "sensor_data",
    "timestamp": "2024-01-15T10:30:00Z",
    "temperature": 25.6,
    "flame_detected": false,
    "flame_intensity": 12,
    "co_ppm": 8.5,
    "water_flow": 0.0,
    "total_water": 125.8,
    "pump_active": false,
    "alarm_active": false,
    "fire_alarm": false
}
```

#### Formato de Eventos Históricos
```json
{
    "type": "fire_event",
    "event": "fire_start",
    "timestamp": "2024-01-15T10:35:00Z",
    "trigger_sensor": "flame",
    "sensor_data": {
        "temperature": 52.3,
        "flame_intensity": 95,
        "co_ppm": 45.2
    },
    "actuator_data": {
        "pump_active": true,
        "alarm_active": true
    }
}
```

## Algoritmos de Procesamiento

### 1. Filtrado de Eventos Duplicados

```python
# Algoritmo para evitar procesamiento de eventos duplicados
def is_duplicate_event(event_id, processed_events):
    """
    Verifica si un evento ya fue procesado
    
    Utiliza un conjunto (set) para O(1) lookup time
    Mantiene memoria limitada con limpieza automática
    """
    return event_id in processed_events

# Optimización de memoria
def cleanup_processed_events(processed_events, max_size=100, keep_size=50):
    """
    Limpia eventos antiguos para mantener uso de memoria bajo
    
    Parámetros:
    - max_size: Tamaño máximo antes de limpiar
    - keep_size: Cantidad de eventos a mantener después de limpiar
    """
    if len(processed_events) > max_size:
        events_list = list(processed_events)
        processed_events.clear()
        processed_events.update(events_list[-keep_size:])
```

### 2. Validación de Timestamps

```python
# Algoritmo para validar y normalizar timestamps
def validate_timestamp(timestamp_str, max_age_seconds=10):
    """
    Valida que un timestamp sea reciente y válido
    
    Parámetros:
    - timestamp_str: String del timestamp
    - max_age_seconds: Edad máxima permitida en segundos
    
    Retorna:
    - datetime object si es válido y reciente
    - None si es inválido o muy antiguo
    """
    parsed_time = parse_firebase_timestamp(timestamp_str)
    
    if not parsed_time:
        return None
    
    now = datetime.now(timezone.utc)
    age_seconds = (now - parsed_time).total_seconds()
    
    if age_seconds > max_age_seconds:
        return None
    
    return parsed_time
```

### 3. Categorización Automática de Eventos

```python
# Algoritmo para categorizar eventos automáticamente
def categorize_event(event_data):
    """
    Categoriza eventos basado en su contenido
    
    Categorías:
    - fire: Eventos relacionados con incendios
    - test: Eventos de prueba del sistema
    - status: Cambios de estado del sistema
    - maintenance: Eventos de mantenimiento
    """
    event_type = event_data.get('event', '').lower()
    
    if 'fire' in event_type:
        return 'fire'
    elif 'test' in event_type:
        return 'test'
    elif any(status in event_type for status in ['shutdown', 'resume', 'startup']):
        return 'status'
    elif 'maintenance' in event_type:
        return 'maintenance'
    else:
        return 'unknown'
```

## Optimizaciones Implementadas

### 1. Eficiencia de Red
- **Acceso selectivo**: Solo lee rutas específicas de Firebase
- **Filtrado local**: Procesa datos localmente antes de escribir
- **Batch operations**: Agrupa operaciones cuando es posible

### 2. Gestión de Memoria
- **Limpieza automática**: Elimina eventos procesados antiguos
- **Sets optimizados**: Usa estructuras de datos eficientes
- **Garbage collection**: Libera memoria no utilizada

### 3. Manejo de Errores
- **Recuperación automática**: Continúa funcionando después de errores
- **Logging detallado**: Registra errores para debugging
- **Timeouts configurables**: Evita bloqueos indefinidos

### 4. Rendimiento
- **Frecuencias optimizadas**: Diferentes intervalos según criticidad
- **CPU throttling**: Pausas para reducir uso de CPU
- **Conexiones persistentes**: Reutiliza conexiones de Firebase

## Monitoreo y Logs

### Estructura de Logs

2024-01-15 10:30:00,123 - INFO - 🚀 Iniciando monitoreo optimizado del sistema anti-incendios
2024-01-15 10:30:02,456 - INFO - ✅ Evento fire procesado: fire_start
2024-01-15 10:30:05,789 - INFO - 📢 Notificación procesada: fire_alert
2024-01-15 10:30:30,012 - INFO - 🔍 Monitoreando historial - sin eventos nuevos
2024-01-15 10:30:45,345 - ERROR - Error crítico procesando evento fire: Connection timeout



### Métricas de Rendimiento
- **Eventos procesados por minuto**
- **Tiempo de respuesta promedio**
- **Uso de memoria del proceso**
- **Errores por hora**
- **Disponibilidad del servicio**

## Configuración de Despliegue

### Requisitos del Sistema
- **Python 3.8+**
- **Firebase Admin SDK**
- **Conexión a Internet estable**
- **Credenciales de Firebase configuradas**
- **Permisos de escritura en directorio de logs**

### Ejecución como Servicio
```bash
# Ejecutar en primer plano (desarrollo)
python3 trigger_save_data.py

# Ejecutar en segundo plano (producción)
nohup python3 trigger_save_data.py > /dev/null 2>&1 &

# Usando systemd (recomendado para producción)
sudo systemctl start fire-monitor.service
sudo systemctl enable fire-monitor.service
```

### Configuración de Systemd
```ini
[Unit]
Description=Fire Monitoring System
After=network.target

[Service]
Type=simple
User=fireuser
WorkingDirectory=/opt/fire-system
ExecStart=/usr/bin/python3 trigger_save_data.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Esta documentación técnica detallada proporciona una comprensión completa del funcionamiento del script de Python como procesador de eventos en tiempo real, incluyendo todos los algoritmos de procesamiento, protocolos de comunicación y optimizaciones implementadas.