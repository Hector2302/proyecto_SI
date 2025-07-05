import firebase_admin
from firebase_admin import credentials
from firebase_admin import db
import random
import time
from datetime import datetime, timedelta
import pytz
import threading
import uuid

# Configuración de Firebase
cred = credentials.ApplicationDefault()
firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://firefighting-system-fed2c-default-rtdb.firebaseio.com'
})

ref = db.reference('/system')

class SystemState:
    def __init__(self):
        # Tiempos y estados
        self.last_fire_time = datetime.now(pytz.utc)
        self.last_test_time = datetime.now(pytz.utc)
        self.last_sensor_error = datetime.now(pytz.utc)
        self.last_co_increase_time = datetime.now(pytz.utc)  # Nuevo: tiempo del último aumento de CO
        self.last_weak_signal_time = datetime.now(pytz.utc)  # Nuevo: tiempo de la última señal débil
        self.fire_duration = 0
        self.co_increase_duration = 0  # Nuevo: duración del aumento de CO
        self.weak_signal_duration = 0  # Nuevo: duración de la señal débil
        self.is_fire = False
        self.is_co_increasing = False  # Nuevo: estado de aumento de CO
        self.is_weak_signal = False  # Nuevo: estado de señal débil
        self.test_in_progress = False
        self.is_shutdown = False  # Nuevo: estado de apagado del sistema
        self.sensor_status = {
            'temperature': 'ok',
            'co_level': 'ok',
            'flame': 'ok',
            'water_flow': 'ok'
        }
        
        # Contadores y mediciones
        self.alarm_duration_sec = 0
        self.pump_runtime = 0
        self.water_total_liters = 0.0
        self.test_water_used = 0.0
        
        # Control de pruebas
        self.test_cooldown = False
        self.test_start_time = None
        
        # Historial
        self.history = []
        
        # Notificaciones
        self.notifications = {}
        self.notification_count = 0
        
        # Control de estados previos para evitar notificaciones duplicadas
        self.previous_override_state = False
        self.previous_shutdown_state = False

state = SystemState()

SENSOR_RANGES = {
    'temperature': {'normal': (20.0, 30.0), 'fire': (80.0, 120.0)},
    'co_level': {'normal': (0, 10), 'fire': (500, 1000), 'increased': (100, 300)},  # Añadido rango para CO aumentado
    'flame': {'normal': (0, 50), 'fire': (800, 1023)},
    'water_flow': {'normal': (0.0, 0.0), 'active': (10.0, 20.0)}
}

# Constantes de tiempo (en segundos)
FIRE_INTERVAL = 7200  # 2 horas
CO_INCREASE_INTERVAL = 5400  # 1.5 horas (cambiado de 10800)
WEAK_SIGNAL_INTERVAL = 1800  # 30 minutos (cambiado de 9000)

def generate_normal_temperature():
    hour = datetime.now().hour
    base_temp = 22.0 if 0 <= hour < 6 else 25.0
    fluctuation = random.uniform(-0.5, 1.5) if 6 <= hour < 18 else random.uniform(-1.0, 0.5)
    return base_temp + fluctuation

def check_manual_commands(data=None):
    commands_ref = ref.child('commands')
    commands = commands_ref.get() or {}
    
    # Verificar comando de prueba del sistema
    if commands.get('trigger_test') and not state.test_in_progress and not state.test_cooldown and not state.is_shutdown:
        start_test()
    
    # Verificar comando de apagado del sistema
    if 'shutdown_system' in commands:
        current_shutdown_state = commands.get('shutdown_system')
        
        # Si el estado de apagado ha cambiado
        if current_shutdown_state != state.is_shutdown:
            state.is_shutdown = current_shutdown_state
            current_time = datetime.now(pytz.utc)
            
            # Obtener información del estado actual del sistema
            status_ref = ref.child('status')
            actuators_ref = ref.child('actuators')
            current_status = status_ref.get() or {}
            current_actuators = actuators_ref.get() or {}
            
            if state.is_shutdown:
                # Notificar que el sistema entra en modo reposo
                create_notification(
                    'system_shutdown',
                    '🔌 Sistema en modo reposo',
                    "El sistema ha entrado en modo reposo por comando manual.",
                    {'initiated_by': 'mobile_app'}
                )
                
                # Registrar en historial con más detalles para la nueva tabla
                log_history_event('system_shutdown', {
                    'timestamp': current_time.isoformat(),
                    'initiated_by': 'mobile_app',
                    'previous_mode': current_status.get('mode', 'unknown'),
                    'alarm_active': current_actuators.get('alarm', {}).get('active', False),
                    'pump_active': current_actuators.get('pump', {}).get('active', False)
                })
                
                print(f"Sistema entrando en modo reposo - {current_time.strftime('%Y-%m-%d %H:%M:%S')}")
            else:
                # Notificar que el sistema sale del modo reposo
                create_notification(
                    'system_startup',
                    '🔌 Sistema activado',
                    "El sistema ha salido del modo reposo y está operativo nuevamente.",
                    {'initiated_by': 'mobile_app'}
                )
                
                # Registrar en historial con más detalles para la nueva tabla
                log_history_event('system_startup', {
                    'timestamp': current_time.isoformat(),
                    'initiated_by': 'mobile_app',
                    'downtime_seconds': (current_time - state.last_fire_time).total_seconds() if hasattr(state, 'last_fire_time') else 0,
                    'new_mode': 'normal'
                })
                
                print(f"Sistema saliendo del modo reposo - {current_time.strftime('%Y-%m-%d %H:%M:%S')}")

def start_test():
    state.test_in_progress = True
    state.test_start_time = datetime.now(pytz.utc)
    state.alarm_duration_sec = 0
    state.pump_runtime = 0
    ref.child('commands').update({
        'trigger_test': True,
        'last_command': {
            'type': 'manual_test',
            'author': 'mobile_app',
            'timestamp': state.test_start_time.isoformat()
        }
    })

def manage_water_flow(data):
    if state.test_in_progress:
        flow_rate = random.uniform(*SENSOR_RANGES['water_flow']['active'])
        state.test_water_used += flow_rate / 60
        data['sensors']['water_flow']['rate_lpm'] = round(flow_rate, 1)
        data['sensors']['water_flow']['total_liters'] = round(state.test_water_used, 1)
    elif state.is_fire:
        flow_rate = random.uniform(*SENSOR_RANGES['water_flow']['active'])
        state.water_total_liters += flow_rate / 60
        data['sensors']['water_flow']['rate_lpm'] = round(flow_rate, 1)
        data['sensors']['water_flow']['total_liters'] = round(state.water_total_liters, 1)
    else:
        data['sensors']['water_flow']['rate_lpm'] = 0.0

def generate_notification_id():
    return f"notification_{str(uuid.uuid4())[:8]}"

def get_notification_priority(notification_type):
    priorities = {
        'fire_alert': 'high',
        'fire_controlled': 'normal',
        'co_alert': 'critical',
        'co_level_normalized': 'normal',
        'system_test': 'low',
        'system_shutdown': 'normal',
        'system_startup': 'normal',
        'sensor_error': 'normal',
        'weak_signal': 'normal',
        'signal_restored': 'low'
    }
    return priorities.get(notification_type, 'normal')

def create_notification(notification_type, title, message, related_data=None):
    notification_id = generate_notification_id()
    current_time = datetime.now(pytz.utc)
    
    notification = {
        'type': notification_type,
        'title': title,
        'message': message,
        'priority': get_notification_priority(notification_type),
        'timestamp': current_time.isoformat(),
        'read': False
    }
    
    # Añadir datos relacionados si existen
    if related_data:
        notification.update(related_data)
    
    # Guardar en el estado local
    state.notifications[notification_id] = notification
    state.notification_count += 1
    
    # Actualizar Firebase
    notifications_data = {
        'active': True,
        'last_updated': current_time.isoformat(),
        'queue': state.notifications
    }
    
    ref.child('notifications').set(notifications_data)
    
    # Auto-eliminación después de 10 segundos
    def delete_notification():
        if notification_id in state.notifications:
            del state.notifications[notification_id]
            # Actualizar Firebase
            current_time = datetime.now(pytz.utc)
            notifications_data = {
                'active': len(state.notifications) > 0,
                'last_updated': current_time.isoformat(),
                'queue': state.notifications
            }
            ref.child('notifications').set(notifications_data)
    
    threading.Timer(10, delete_notification).start()
    
    return notification_id

def log_history_event(event_type, event_data):
    history_entry = {
        'event_type': event_type,
        'timestamp': datetime.now(pytz.utc).isoformat(),
        'data': event_data.copy()
    }
    
    state.history.append(history_entry)
    
    # Mantener máximo 100 eventos
    if len(state.history) > 100:
        state.history.pop(0)
    
    ref.child('history').set(state.history)
    
    # Auto-eliminación después de 10 segundos
    def delete_event():
        if history_entry in state.history:
            state.history.remove(history_entry)
            ref.child('history').set(state.history)
    threading.Timer(10, delete_event).start()

def handle_system_events(data):
    current_time = datetime.now(pytz.utc)
    
    # Si el sistema está en modo reposo, no procesar eventos normales
    if state.is_shutdown:
        # Establecer modo de sistema en reposo
        data['status']['mode'] = 'standby'
        # Desactivar actuadores
        data['actuators']['alarm']['active'] = False
        data['actuators']['pump']['active'] = False
        return data
    
    # Verificar comando de desactivación de alarmas
    commands = data['commands']
    if 'override_alarm' in commands:
        current_override_state = commands.get('override_alarm')
        
        # Solo crear notificación si el estado ha cambiado
        if current_override_state != state.previous_override_state:
            if current_override_state:
                # Desactivar todas las alarmas y notificaciones
                data['actuators']['alarm']['active'] = False
                data['status']['emergency_override'] = True
                
                # Crear notificación de override
                create_notification(
                    'alarm_override',
                    '🔕 Alarmas desactivadas',
                    "Las alarmas han sido desactivadas manualmente desde la app móvil.",
                    {'initiated_by': 'mobile_app'}
                )
                
                print(f"Alarmas desactivadas manualmente - {datetime.now(pytz.utc).strftime('%Y-%m-%d %H:%M:%S')}")
            else:
                # Reactivar alarmas
                data['status']['emergency_override'] = False
                
                create_notification(
                    'alarm_reactivated',
                    '🔔 Alarmas reactivadas',
                    "Las alarmas han sido reactivadas y el sistema está operativo.",
                    {'initiated_by': 'mobile_app'}
                )
                
                print(f"Alarmas reactivadas - {datetime.now(pytz.utc).strftime('%Y-%m-%d %H:%M:%S')}")
            
            # Actualizar el estado previo
            state.previous_override_state = current_override_state
        
        # Aplicar el estado actual sin generar notificaciones adicionales
        if current_override_state:
            data['actuators']['alarm']['active'] = False
            data['status']['emergency_override'] = True
        else:
            data['status']['emergency_override'] = False

    # Control de incendio - cada 2 horas
    if not state.is_fire and (current_time - state.last_fire_time).total_seconds() >= FIRE_INTERVAL:
        state.is_fire = True
        state.fire_duration = random.randint(300, 600)  # 5-10 minutos
        state.last_fire_time = current_time
        state.alarm_duration_sec = 0
        state.pump_runtime = 0
        print(f"Iniciando simulación de incendio - {current_time.strftime('%Y-%m-%d %H:%M:%S')}")
        
        # Crear notificación de inicio de incendio
        flame_intensity = random.randint(*SENSOR_RANGES['flame']['fire'])
        create_notification(
            'fire_alert',
            '🔥 Incendio detectado',
            f"Llama detectada con intensidad {flame_intensity}. Alarma y bomba activadas.",
            {'flame_intensity': flame_intensity}
        )
    
    # Control de aumento de CO - cada 3 horas
    if not state.is_co_increasing and not state.is_fire and (current_time - state.last_co_increase_time).total_seconds() >= CO_INCREASE_INTERVAL:
        state.is_co_increasing = True
        state.co_increase_duration = random.randint(180, 300)  # 3-5 minutos
        state.last_co_increase_time = current_time
        print(f"Iniciando simulación de aumento de CO - {current_time.strftime('%Y-%m-%d %H:%M:%S')}")
        
        # Crear notificación de aumento de CO
        co_level = random.randint(*SENSOR_RANGES['co_level']['increased'])
        create_notification(
            'co_alert',
            '🛑 CO en niveles peligrosos',
            f"Nivel de monóxido de carbono alcanzó {co_level} ppm. Riesgo de intoxicación.",
            {'co_ppm': co_level}
        )
    
    # Control de señal débil - cada 2.5 horas
    if not state.is_weak_signal and not state.is_fire and not state.is_co_increasing and (current_time - state.last_weak_signal_time).total_seconds() >= WEAK_SIGNAL_INTERVAL:
        state.is_weak_signal = True
        state.weak_signal_duration = random.randint(120, 240)  # 2-4 minutos
        state.last_weak_signal_time = current_time
        print(f"Iniciando simulación de señal débil - {current_time.strftime('%Y-%m-%d %H:%M:%S')}")
        
        # Crear notificación de señal débil
        signal_strength = random.randint(-95, -85)  # Valores típicos de señal débil en dBm
        create_notification(
            'weak_signal',
            '📶 Señal WiFi débil',
            f"La intensidad de la señal WiFi ha caído a {signal_strength} dBm. Posible pérdida de conectividad.",
            {'signal_strength_dbm': signal_strength}
        )
        
        # Registrar en historial
        log_history_event('weak_signal_detected', {
            'timestamp': current_time.isoformat(),
            'signal_strength_dbm': signal_strength
        })
    
    # Manejo de incendio activo
    if state.is_fire:
        data = activate_fire_mode(data)
        if state.fire_duration <= 0:
            finalize_fire(data)
    
    # Manejo de aumento de CO activo
    elif state.is_co_increasing:
        data = activate_co_increase_mode(data)
        if state.co_increase_duration <= 0:
            finalize_co_increase(data)
    
    # Manejo de señal débil activa
    elif state.is_weak_signal:
        data = activate_weak_signal_mode(data)
        if state.weak_signal_duration <= 0:
            finalize_weak_signal(data)
    
    # Manejo de pruebas
    if state.test_in_progress:
        handle_test(data)
    
    # Actualizar duración de alarmas y bombas (después de establecer su estado)
    if data['actuators']['alarm']['active']:
        state.alarm_duration_sec += 1
    if data['actuators']['pump']['active']:
        state.pump_runtime += 1
    
    return data

def activate_fire_mode(data):
    fire_data = {
        'temperature': round(random.uniform(*SENSOR_RANGES['temperature']['fire']), 1),
        'co_level': random.randint(*SENSOR_RANGES['co_level']['fire']),
        'flame_intensity': random.randint(*SENSOR_RANGES['flame']['fire']),
        'water_flow': {
            'rate_lpm': round(random.uniform(*SENSOR_RANGES['water_flow']['active']), 1),
            'total_liters': round(state.water_total_liters, 1)
        }
    }
    
    data['sensors']['temperature'].update({
        'value': fire_data['temperature'],
        'status': 'ok'
    })
    
    data['sensors']['co_level']['ppm'] = fire_data['co_level']
    data['sensors']['flame'].update({
        'detected': True,
        'intensity': fire_data['flame_intensity']
    })
    
    data['sensors']['water_flow'].update(fire_data['water_flow'])
    
    data['actuators']['alarm']['active'] = True
    data['actuators']['pump']['active'] = True
    data['status']['mode'] = 'emergency'
    
    state.fire_duration -= 1
    
    return data

# Nueva función para manejar el aumento de CO
def activate_co_increase_mode(data):
    co_data = {
        'co_level': random.randint(*SENSOR_RANGES['co_level']['increased']),
    }
    
    data['sensors']['co_level']['ppm'] = co_data['co_level']
    
    # Activar alarma pero no la bomba
    data['actuators']['alarm']['active'] = True
    data['actuators']['pump']['active'] = False
    data['status']['mode'] = 'warning'
    
    state.co_increase_duration -= 1
    
    return data

def finalize_fire(data):
    state.is_fire = False
    state.alarm_duration_sec = 0
    state.pump_runtime = 0
    
    # Registrar en historial
    log_history_event('fire_controlled', {
        'alarm': data['actuators']['alarm'],
        'pump': data['actuators']['pump'],
        'temperature': data['sensors']['temperature'],
        'co_level': data['sensors']['co_level'],
        'flame': data['sensors']['flame'],
        'water_flow': data['sensors']['water_flow'],
        'water_used_liters': round(state.water_total_liters, 1)
    })
    
    # Crear notificación de fin de incendio después de 1 segundo
    def create_fire_notification():
        # Buscar el evento en el historial para obtener los datos más actualizados
        fire_event = None
        for event in state.history:
            if event['event_type'] == 'fire_controlled':
                fire_event = event
                break
        
        water_used = round(state.water_total_liters, 1)
        if fire_event and 'data' in fire_event and 'water_used_liters' in fire_event['data']:
            water_used = fire_event['data']['water_used_liters']
        
        create_notification(
            'fire_controlled',
            '✅ Incendio controlado',
            f"Incendio controlado después de {state.alarm_duration_sec} segundos. Agua utilizada: {water_used} litros.",
            {
                'duration_sec': state.alarm_duration_sec,
                'water_used_liters': water_used
            }
        )
    
    # Programar la notificación para 1 segundo después
    threading.Timer(1, create_fire_notification).start()
    
    print(f"Finalizada simulación de incendio - {datetime.now(pytz.utc).strftime('%Y-%m-%d %H:%M:%S')}")

# Nueva función para finalizar el aumento de CO
def finalize_co_increase(data):
    state.is_co_increasing = False
    state.alarm_duration_sec = 0
    
    # Registrar en historial
    log_history_event('co_level_normalized', {
        'alarm': data['actuators']['alarm'],
        'co_level': data['sensors']['co_level'],
    })
    
    # Crear notificación de normalización de CO
    create_notification(
        'co_level_normalized',
        '✅ Niveles de CO normalizados',
        f"Los niveles de monóxido de carbono han vuelto a la normalidad. Valor actual: {data['sensors']['co_level']['ppm']} ppm.",
        {'co_ppm': data['sensors']['co_level']['ppm']}
    )
    
    print(f"Finalizada simulación de aumento de CO - {datetime.now(pytz.utc).strftime('%Y-%m-%d %H:%M:%S')}")

def handle_test(data):
    elapsed = (datetime.now(pytz.utc) - state.test_start_time).total_seconds()
    
    # Si es el inicio del test (menos de 1 segundo)
    if elapsed < 1:
        # Crear notificación de inicio de prueba
        create_notification(
            'system_test',
            '🛠 Prueba del sistema iniciada',
            "Se ha iniciado una prueba del sistema desde la app móvil.",
            {'initiated_by': 'mobile_app'}
        )
    
    if elapsed >= 20:
        # PRIMERO activar alarma y bomba, LUEGO registrar en historial
        data['actuators']['alarm']['active'] = True
        data['actuators']['pump']['active'] = True
        data['status']['mode'] = 'test'
        
        test_data = {
            'alarm': data['actuators']['alarm'],
            'pump': data['actuators']['pump'],
            'temperature': data['sensors']['temperature'],
            'co_level': data['sensors']['co_level'],
            'flame': data['sensors']['flame'],
            'water_flow': data['sensors']['water_flow'],
            'water_used_liters': round(state.test_water_used, 1)
        }
        
        state.test_in_progress = False
        state.alarm_duration_sec = 0
        state.pump_runtime = 0
        state.test_cooldown = True
        water_used = round(state.test_water_used, 1)
        ref.child('commands/trigger_test').set(False)
        
        # Ahora registrar en el historial con los estados correctos
        log_history_event('system_test', test_data)
        
        # Crear notificación de fin de prueba después de 1 segundo
        def create_test_notification():
            # Buscar el evento en el historial para obtener los datos más actualizados
            test_event = None
            for event in state.history:
                if event['event_type'] == 'system_test':
                    test_event = event
                    break
            
            water_used_value = water_used
            if test_event and 'data' in test_event and 'water_used_liters' in test_event['data']:
                water_used_value = test_event['data']['water_used_liters']
            
            create_notification(
                'system_test',
                '🛠 Prueba del sistema completada',
                f"Se completó la prueba del sistema. Agua utilizada: {water_used_value} litros.",
                {'water_used_liters': water_used_value}
            )
        
        # Programar la notificación para 1 segundo después
        threading.Timer(1, create_test_notification).start()
        
        # Resetear el contador de agua utilizada después de crear la notificación
        state.test_water_used = 0.0
        
        threading.Timer(5, lambda: setattr(state, 'test_cooldown', False)).start()
    else:
        data['actuators']['alarm']['active'] = True
        data['actuators']['pump']['active'] = True
        data['status']['mode'] = 'test'
        data['commands']['trigger_test'] = True

def check_sensor_errors(data):
    current_time = datetime.now(pytz.utc)
    
    # Simular error de sensor cada 24 horas (86400 segundos)
    if (current_time - state.last_sensor_error).total_seconds() >= 86400:
        # Elegir un sensor aleatorio para simular error
        sensors = ['temperature', 'co_level', 'flame', 'water_flow']
        error_sensor = random.choice(sensors)
        
        # Actualizar estado del sensor
        state.sensor_status[error_sensor] = 'error'
        data['sensors'][error_sensor]['status'] = 'error'
        
        # Crear notificación de error de sensor
        sensor_names = {
            'temperature': 'Temperatura',
            'co_level': 'Monóxido de Carbono',
            'flame': 'Llama',
            'water_flow': 'Flujo de Agua'
        }
        
        create_notification(
            'sensor_error',
            f"⚠️ Error de sensor: {sensor_names[error_sensor]}",
            f"Sensor de {sensor_names[error_sensor]} desconectado desde {current_time.strftime('%H:%M')}.",
            {'related_sensor': error_sensor}
        )
        
        state.last_sensor_error = current_time
        
        # Simular recuperación del sensor después de 30 segundos
        def recover_sensor():
            state.sensor_status[error_sensor] = 'ok'
            print(f"Sensor {error_sensor} recuperado - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        
        threading.Timer(30, recover_sensor).start()
    
    return data

# Nueva función para manejar el modo de señal débil
def activate_weak_signal_mode(data):
    # Simular señal débil en el rango de -95 a -85 dBm
    signal_strength = random.randint(-95, -85)
    
    # Actualizar datos de conectividad
    if 'connectivity' not in data['status']:
        data['status']['connectivity'] = {}
    
    data['status']['connectivity']['wifi_connected'] = True
    data['status']['connectivity']['signal_strength_dbm'] = signal_strength
    data['status']['mode'] = 'warning'
    
    # Decrementar la duración del evento
    state.weak_signal_duration -= 1
    
    return data

# Nueva función para finalizar el evento de señal débil
def finalize_weak_signal(data):
    state.is_weak_signal = False
    
    # Restaurar señal a valores normales (-65 a -55 dBm)
    signal_strength = random.randint(-65, -55)
    
    if 'connectivity' not in data['status']:
        data['status']['connectivity'] = {}
    
    data['status']['connectivity']['wifi_connected'] = True
    data['status']['connectivity']['signal_strength_dbm'] = signal_strength
    data['status']['mode'] = 'normal'
    
    # Registrar en historial
    log_history_event('signal_restored', {
        'timestamp': datetime.now(pytz.utc).isoformat(),
        'previous_signal_strength': data['status']['connectivity']['signal_strength_dbm'],
        'new_signal_strength': signal_strength,
        'duration_seconds': state.weak_signal_duration
    })
    
    # Crear notificación de señal restaurada
    create_notification(
        'signal_restored',
        '📶 Señal WiFi restaurada',
        f"La intensidad de la señal WiFi se ha normalizado a {signal_strength} dBm.",
        {'signal_strength_dbm': signal_strength}
    )
    
    print(f"Finalizada simulación de señal débil - {datetime.now(pytz.utc).strftime('%Y-%m-%d %H:%M:%S')}")
    
    return data

def update_firebase(data):
    try:
        # Verificar errores de sensores
        data = check_sensor_errors(data)
        
        # Leer el estado actual de shutdown_system desde Firebase
        current_commands = ref.child('commands').get() or {}
        current_shutdown = current_commands.get('shutdown_system', False)
        
        # Preservar el estado de shutdown_system
        data['commands']['shutdown_system'] = current_shutdown
        
        # Actualizar Firebase con todos los datos
        ref.update({
            'sensors': data['sensors'],
            'actuators': data['actuators'],
            'status': data['status'],
            'commands': data['commands']
        })
        
        print("Actualización exitosa -", datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
        return True
    except Exception as e:
        print(f"Error de conexión: {str(e)}")
        return False

# Bucle principal
while True:
    try:
        current_data = {
            'sensors': {
                'temperature': {'value': round(generate_normal_temperature(), 1), 'unit': 'Celsius', 'status': 'ok'},
                'co_level': {'ppm': random.randint(*SENSOR_RANGES['co_level']['normal']), 'status': 'ok'},
                'flame': {'detected': False, 'intensity': random.randint(*SENSOR_RANGES['flame']['normal']), 'status': 'ok'},
                'water_flow': {'rate_lpm': 0.0, 'total_liters': 0.0, 'status': 'ok'}
            },
            'actuators': {
                'alarm': {'active': False, 'duration_sec': state.alarm_duration_sec},
                'pump': {'active': False, 'runtime_sec': state.pump_runtime}
            },
            'status': {
                'mode': 'normal',
                'emergency_override': False,
                'last_update': datetime.now(pytz.utc).isoformat(),
                'connectivity': {
                    'wifi_connected': True,
                    'signal_strength_dbm': random.randint(-70, -50)
                }
            },
            'commands': {
                'trigger_test': False,
                # NO resetear shutdown_system automáticamente
                # 'shutdown_system': False,  # ELIMINAR ESTA LÍNEA
                'override_alarm': False,
                'last_command': {
                    'type': "none",
                    'author': "system",
                    'timestamp': datetime.now(pytz.utc).isoformat()
                }
            }
        }
        
        check_manual_commands(current_data)
        
        # Si el sistema está en shutdown, desactivar todos los sensores
        if state.is_shutdown:
            current_data['sensors']['temperature']['status'] = 'disabled'
            current_data['sensors']['co_level']['status'] = 'disabled'
            current_data['sensors']['flame']['status'] = 'disabled'
            current_data['sensors']['water_flow']['status'] = 'disabled'
            current_data['status']['mode'] = 'shutdown'
            # No procesar eventos del sistema cuando está en shutdown
        else:
            manage_water_flow(current_data)
            current_data = handle_system_events(current_data)

        current_data['actuators']['alarm']['duration_sec'] = state.alarm_duration_sec
        current_data['actuators']['pump']['runtime_sec'] = state.pump_runtime
        
        update_firebase(current_data)
        
        time.sleep(1)
        
    except Exception as e:
        print(f"Error crítico: {str(e)}")
        time.sleep(5)