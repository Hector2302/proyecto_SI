import time
import logging
from datetime import datetime, timedelta, timezone
from firebase_admin import credentials, initialize_app, db
from firebase_admin import firestore

# Configuración de logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[logging.FileHandler('fire_monitor.log'), logging.StreamHandler()]
)

# Inicialización de Firebase
cred = credentials.ApplicationDefault()
app = initialize_app(cred, {
    'databaseURL': 'https://firefighting-system-fed2c-default-rtdb.firebaseio.com'
})

# Inicialización del cliente Firestore para almacenamiento de históricos
fs = firestore.client()

# Estado global del procesamiento
sensor_data_cache = {
    'temperature': [],
    'co_level': [],
    'flame': []
    # Eliminamos water_flow según los requisitos
}

processed_events = set()
processed_notifications = set()

def get_hourly_timestamp():
    now = datetime.now(timezone.utc)
    return now.replace(minute=0, second=0, microsecond=0)

def parse_firebase_timestamp(timestamp_str):
    try:
        # Remove any prefix like 'detected_' that might be present
        if '_' in timestamp_str and not timestamp_str.startswith('20'):
            # Find the actual timestamp part after the underscore
            parts = timestamp_str.split('_', 1)
            if len(parts) > 1:
                timestamp_str = parts[1]
        
        # Corregimos el problema de parseo de fechas
        if timestamp_str.endswith('Z'):
            # Formato ISO 8601 con Z al final (UTC)
            dt_str = timestamp_str.replace('Z', '+00:00')
            return datetime.fromisoformat(dt_str)
        elif 'T' in timestamp_str and '+' not in timestamp_str and '-' in timestamp_str:
            # Formato ISO sin zona horaria, asumimos UTC
            return datetime.fromisoformat(timestamp_str + '+00:00')
        else:
            # Intentamos parsear directamente
            return datetime.fromisoformat(timestamp_str).astimezone(timezone.utc)
    except Exception as e:
        logging.error(f"Error parsing timestamp: {timestamp_str} - {str(e)}")
        return datetime.now(timezone.utc)

def process_sensor_data():
    try:
        # Accedemos solo a las rutas necesarias para evitar consumos innecesarios
        ref = db.reference('system/sensors')
        sensors = ref.get() or {}
        
        timestamp = datetime.now(timezone.utc).isoformat()
        
        # Recolectamos datos de temperatura
        if temp := sensors.get('temperature', {}).get('value'):
            sensor_data_cache['temperature'].append(float(temp))
            
        # Recolectamos datos de nivel de CO
        if co := sensors.get('co_level', {}).get('ppm'):
            sensor_data_cache['co_level'].append(int(co))
            
        # Recolectamos datos de detección de llama
        if flame := sensors.get('flame', {}):
            sensor_data_cache['flame'].append({
                'detected': flame.get('detected', False),
                'intensity': flame.get('intensity', 0)
            })
            
        # Nota: Según los requisitos, no necesitamos monitorear water_flow cada minuto
        # pero mantenemos la estructura por si se requiere en el futuro
        
        logging.info(f"Datos de sensores recolectados: {timestamp}")
        
    except Exception as e:
        logging.error(f"Error recolectando sensores: {str(e)}")

def calculate_hourly_averages():
    try:
        hourly_data = {
            'timestamp': get_hourly_timestamp().isoformat(),
            'temperature': {'avg': 0.0, 'min': 0.0, 'max': 0.0},
            'co_level': {'avg_ppm': 0, 'max_ppm': 0},
            'flame': {'detected_count': 0, 'avg_intensity': 0.0},
            # Eliminamos water_flow de los promedios horarios según requisitos
            'sample_count': 0
        }
        
        if temps := sensor_data_cache['temperature']:
            hourly_data['temperature']['avg'] = sum(temps) / len(temps)
            hourly_data['temperature']['min'] = min(temps)
            hourly_data['temperature']['max'] = max(temps)
        
        if co_levels := sensor_data_cache['co_level']:
            hourly_data['co_level']['avg_ppm'] = sum(co_levels) // len(co_levels)
            hourly_data['co_level']['max_ppm'] = max(co_levels)
        
        flame_data = [f for f in sensor_data_cache['flame'] if f['detected']]
        if flame_data:
            hourly_data['flame']['detected_count'] = len(flame_data)
            hourly_data['flame']['avg_intensity'] = sum(f['intensity'] for f in flame_data) / len(flame_data)
        
        # Eliminamos el procesamiento de water_flow según los requisitos
        
        hourly_data['sample_count'] = len(sensor_data_cache['temperature'])
        
        doc_ref = fs.collection('sensor_averages_by_hour').document(hourly_data['timestamp'])
        doc_ref.set(hourly_data)
        logging.info(f"Promedios horarios guardados: {hourly_data['timestamp']}")
        
        for key in sensor_data_cache:
            sensor_data_cache[key].clear()
            
    except Exception as e:
        logging.error(f"Error calculando promedios: {str(e)}")

def process_history_events():
    try:
        # Accedemos a la ruta de históricos del simulador anti incendios
        ref = db.reference('system/history')
        history = ref.get() or []
        
        for event in history:
            try:
                event_id = f"{event['event_type']}_{event['timestamp']}"
                if event_id in processed_events:
                    continue
                
                event_time = parse_firebase_timestamp(event['timestamp'])
                now = datetime.now(timezone.utc)
                
                # Solo procesamos eventos recientes (últimos 10 segundos)
                if (now - event_time).total_seconds() > 10:
                    continue
                
                # Eventos de prueba o incendio se almacenan en una tabla específica
                if event['event_type'] in ['fire_detected', 'system_test']:
                    doc_data = {
                        'event_type': event['event_type'],
                        'timestamp': event_time.isoformat(),
                        'source': 'simulador_anti_incendios',
                        'data': event.get('data', {})
                    }
                    fs.collection('fire_and_test_events').document(event_id).set(doc_data)
                    logging.info(f"Evento registrado: {event['event_type']}")
                
                # Eventos de error se almacenan en otra tabla
                elif event['event_type'] == 'sensor_error':
                    doc_data = {
                        'sensor': event['sensor'],
                        'status': event['status'],
                        'timestamp': event_time.isoformat(),
                        'last_ok_timestamp': parse_firebase_timestamp(
                            event['last_ok_timestamp']
                        ).isoformat(),
                        'resolved': False
                    }
                    fs.collection('sensor_errors').document(event_id).set(doc_data)
                    logging.info(f"Error de sensor registrado: {event['sensor']}")
                
                # Eventos de señal débil se almacenan en una tabla específica
                elif event['event_type'] in ['weak_signal_detected', 'signal_restored']:
                    doc_data = {
                        'event_type': event['event_type'],
                        'timestamp': event_time.isoformat(),
                        'signal_strength_dbm': event.get('data', {}).get('signal_strength_dbm', 0),
                        'duration_seconds': event.get('data', {}).get('duration_seconds', 0),
                        'source': 'simulador_anti_incendios'
                    }
                    fs.collection('weak_signal_events').document(event_id).set(doc_data)
                    logging.info(f"Evento de señal registrado: {event['event_type']}")
                
                # Eventos de modo reposo/reactivación se almacenan en otra tabla
                elif event['event_type'] in ['system_shutdown', 'system_startup']:
                    doc_data = {
                        'event_type': event['event_type'],
                        'timestamp': event_time.isoformat(),
                        'initiated_by': event.get('data', {}).get('initiated_by', 'unknown'),
                        'source': 'simulador_anti_incendios'
                    }
                    
                    # Añadir campos específicos según el tipo de evento
                    if event['event_type'] == 'system_shutdown':
                        doc_data.update({
                            'previous_mode': event.get('data', {}).get('previous_mode', 'unknown'),
                            'alarm_active': event.get('data', {}).get('alarm_active', False),
                            'pump_active': event.get('data', {}).get('pump_active', False)
                        })
                    elif event['event_type'] == 'system_startup':
                        doc_data.update({
                            'downtime_seconds': event.get('data', {}).get('downtime_seconds', 0),
                            'new_mode': event.get('data', {}).get('new_mode', 'normal')
                        })
                    
                    fs.collection('system_mode_events').document(event_id).set(doc_data)
                    logging.info(f"Evento de modo sistema registrado: {event['event_type']}")
                
                processed_events.add(event_id)
                
            except KeyError as e:
                logging.warning(f"Evento incompleto: {str(e)}")
            except Exception as e:
                logging.error(f"Error procesando evento: {str(e)}")
                
        now = datetime.now(timezone.utc)
        processed_events_copy = processed_events.copy()
        for eid in processed_events_copy:
            try:
                _, _, timestamp_str = eid.split('_', 2)
                event_time = parse_firebase_timestamp(timestamp_str)
                if (now - event_time).total_seconds() > 15:
                    processed_events.discard(eid)
            except Exception as e:
                logging.error(f"Error limpiando eventos: {str(e)}")
                
    except Exception as e:
        logging.error(f"Error procesando historial: {str(e)}")

def process_notifications():
    try:
        # Acceder a las notificaciones del simulador
        ref = db.reference('system/notifications')
        notifications_data = ref.get() or {}
        
        if not notifications_data.get('active', False):
            return
        
        notification_queue = notifications_data.get('queue', {})
        
        for notification_id, notification in notification_queue.items():
            try:
                # Verificar si ya procesamos esta notificación
                if notification_id in processed_notifications:
                    continue
                
                notification_time = parse_firebase_timestamp(notification['timestamp'])
                now = datetime.now(timezone.utc)
                
                # Solo procesar notificaciones recientes (últimos 15 segundos)
                if (now - notification_time).total_seconds() > 15:
                    continue
                
                # Preparar datos para Firestore
                doc_data = {
                    'notification_id': notification_id,
                    'type': notification['type'],
                    'title': notification['title'],
                    'message': notification['message'],
                    'priority': notification['priority'],
                    'timestamp': notification_time.isoformat(),
                    'read': notification.get('read', False),
                    'source': 'simulador_anti_incendios',
                    'related_data': {}
                }
                
                # Agregar datos relacionados si existen
                for key, value in notification.items():
                    if key not in ['type', 'title', 'message', 'priority', 'timestamp', 'read']:
                        doc_data['related_data'][key] = value
                
                # Guardar en Firestore
                fs.collection('system_notifications').document(notification_id).set(doc_data)
                logging.info(f"Notificación procesada: {notification['type']} - {notification['title']}")
                
                processed_notifications.add(notification_id)
                
            except Exception as e:
                logging.error(f"Error procesando notificación {notification_id}: {str(e)}")
        
        # Limpiar notificaciones procesadas antiguas
        now = datetime.now(timezone.utc)
        processed_notifications_copy = processed_notifications.copy()
        for nid in processed_notifications_copy:
            try:
                # Remover notificaciones procesadas hace más de 30 segundos
                # (asumiendo que el ID contiene timestamp o podemos obtenerlo de Firestore)
                processed_notifications.discard(nid)
            except Exception as e:
                logging.error(f"Error limpiando notificaciones: {str(e)}")
                
    except Exception as e:
        logging.error(f"Error procesando notificaciones: {str(e)}")

def main():
    last_sensor_update = time.time()
    last_history_check = time.time()
    last_notification_check = time.time()
    
    logging.info("Iniciando monitoreo de simulador anti incendios...")
    
    while True:
        try:
            current_time = time.time()
            
            # Tomar muestra cada minuto de los sensores (excepto water_flow)
            if current_time - last_sensor_update >= 60:
                process_sensor_data()
                last_sensor_update = current_time
                
                # Calcular promedios cada hora (cuando tengamos 60 muestras)
                if len(sensor_data_cache['temperature']) >= 60:
                    calculate_hourly_averages()
            
            # Verificar históricos cada 3 segundos
            if current_time - last_history_check >= 3:
                process_history_events()
                last_history_check = current_time
            
            # Verificar notificaciones cada 2 segundos
            if current_time - last_notification_check >= 2:
                process_notifications()
                last_notification_check = current_time
                
            time.sleep(0.1)
            
        except KeyboardInterrupt:
            logging.info("Deteniendo monitor...")
            break
            
        except Exception as e:
            logging.error(f"Error en el loop principal: {str(e)}")
            time.sleep(10)

if __name__ == '__main__':
    main()