import time
import logging
from datetime import datetime, timezone
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
processed_events = set()
last_log_time = 0
empty_history_logged = False

def parse_firebase_timestamp(timestamp_str):
    try:
        if not timestamp_str or timestamp_str.strip() == '':
            return None  # Retornar None en lugar de timestamp actual
        
        if timestamp_str.endswith('Z'):
            dt_str = timestamp_str.replace('Z', '+00:00')
            return datetime.fromisoformat(dt_str)
        elif 'T' in timestamp_str and '+' not in timestamp_str and '-' in timestamp_str:
            return datetime.fromisoformat(timestamp_str + '+00:00')
        else:
            return datetime.fromisoformat(timestamp_str).astimezone(timezone.utc)
    except Exception as e:
        return None  # Retornar None en lugar de timestamp actual

def process_history_events():
    global last_log_time, empty_history_logged
    
    try:
        # Acceder solo a la ruta de historial para minimizar consumo
        ref = db.reference('system/history')
        history_data = ref.get()
        
        # Manejar caso cuando history está vacío (log solo una vez)
        if not history_data:
            if not empty_history_logged:
                logging.info("Historial vacío - esperando eventos...")
                empty_history_logged = True
            return
        
        # Reset flag si hay datos
        empty_history_logged = False
        
        # Procesar cada categoría (fire, test, status)
        categories = ['fire', 'test', 'status']
        events_processed = 0
        
        for category in categories:
            category_events = history_data.get(category, {})
            
            # Si la categoría está vacía, continuar sin log
            if not category_events:
                continue
            
            for event_key, event_data in category_events.items():
                try:
                    # FILTRO 1: Ignorar eventos placeholder de Firebase
                    if 'placeholder' in event_key.lower():
                        continue
                    
                    # FILTRO 2: Ignorar eventos sin datos reales
                    if not isinstance(event_data, dict) or not event_data:
                        continue
                    
                    # Crear ID único para el evento
                    event_id = f"{category}_{event_key}"
                    
                    # Si ya procesamos este evento, saltarlo inmediatamente
                    if event_id in processed_events:
                        continue
                    
                    # Validar que el timestamp no esté vacío
                    timestamp_str = event_data.get('timestamp', '')
                    event_time = parse_firebase_timestamp(timestamp_str)
                    
                    # Si no hay timestamp válido, saltar SIN LOG (son placeholders)
                    if not event_time:
                        continue
                    
                    # Verificar que el evento sea reciente (últimos 10 segundos)
                    now = datetime.now(timezone.utc)
                    if (now - event_time).total_seconds() > 10:
                        continue
                    
                    # Validar que el evento tenga datos mínimos requeridos
                    event_type = event_data.get('event', '')
                    if not event_type:
                        continue
                    
                    # Preparar datos base del evento
                    doc_data = {
                        'event_type': event_type,
                        'category': category,
                        'timestamp': event_time.isoformat(),
                        'source': 'sistema_anti_incendios',
                        'event_key': event_key
                    }
                    
                    # Agregar datos específicos del evento
                    if 'trigger_sensor' in event_data:
                        doc_data['trigger_sensor'] = event_data['trigger_sensor']
                    
                    if 'duration_seconds' in event_data:
                        doc_data['duration_seconds'] = event_data['duration_seconds']
                    
                    if 'water_used' in event_data:
                        doc_data['water_used'] = event_data['water_used']
                    
                    # Para eventos finales (_end), incluir todos los datos de sensores y actuadores
                    if '_end' in event_type:
                        if 'sensor_data' in event_data:
                            doc_data['sensor_data'] = event_data['sensor_data']
                        
                        if 'actuator_data' in event_data:
                            doc_data['actuator_data'] = event_data['actuator_data']
                        
                        if 'system_status' in event_data:
                            doc_data['system_status'] = event_data['system_status']
                    
                    # Guardar en Firestore según la categoría
                    if category == 'fire':
                        collection_name = 'fire_events'
                    elif category == 'test':
                        collection_name = 'test_events'
                    else:  # status
                        collection_name = 'system_status_events'
                    
                    fs.collection(collection_name).document(event_id).set(doc_data)
                    
                    # LOG SOLO CUANDO HAY EVENTOS REALES
                    logging.info(f"✅ Evento {category} procesado: {event_type}")
                    events_processed += 1
                    
                    # Marcar como procesado SOLO después de guardarlo exitosamente
                    processed_events.add(event_id)
                    
                except Exception as e:
                    # Solo log errores críticos, no warnings
                    logging.error(f"Error crítico procesando evento {category}: {str(e)}")
        
        # Log de estado solo cada 30 segundos si no hay eventos
        current_time = time.time()
        if events_processed == 0 and (current_time - last_log_time) > 30:
            logging.info("🔍 Monitoreando historial - sin eventos nuevos")
            last_log_time = current_time
        
        # Limpiar eventos procesados antiguos (más de 100 eventos para liberar memoria)
        if len(processed_events) > 100:
            # Mantener solo los últimos 50 eventos
            processed_events_list = list(processed_events)
            processed_events.clear()
            processed_events.update(processed_events_list[-50:])
                
    except Exception as e:
        logging.error(f"Error crítico procesando historial: {str(e)}")

def process_notifications():
    """Procesar notificaciones del sistema (simplificado)"""
    try:
        ref = db.reference('system/notifications')
        notifications_data = ref.get() or {}
        
        if not notifications_data.get('active', False):
            return
        
        notification_queue = notifications_data.get('queue', {})
        notifications_processed = 0
        
        for notification_id, notification in notification_queue.items():
            try:
                # Filtrar placeholders
                if 'placeholder' in notification_id.lower():
                    continue
                
                timestamp_str = notification.get('timestamp', '')
                notification_time = parse_firebase_timestamp(timestamp_str)
                
                if not notification_time:
                    continue
                
                now = datetime.now(timezone.utc)
                if (now - notification_time).total_seconds() > 15:
                    continue
                
                doc_data = {
                    'notification_id': notification_id,
                    'type': notification.get('type', ''),
                    'title': notification.get('title', ''),
                    'message': notification.get('message', ''),
                    'priority': notification.get('priority', 'normal'),
                    'timestamp': notification_time.isoformat(),
                    'read': notification.get('read', False),
                    'source': 'sistema_anti_incendios'
                }
                
                fs.collection('system_notifications').document(notification_id).set(doc_data)
                logging.info(f"📢 Notificación procesada: {notification.get('type', '')}")
                notifications_processed += 1
                
            except Exception as e:
                logging.error(f"Error procesando notificación {notification_id}: {str(e)}")
        
        # Solo log si hay notificaciones procesadas
        if notifications_processed > 0:
            logging.info(f"📊 {notifications_processed} notificaciones procesadas")
                
    except Exception as e:
        logging.error(f"Error procesando notificaciones: {str(e)}")

def main():
    last_history_check = time.time()
    
    logging.info("🚀 Iniciando monitoreo optimizado del sistema anti-incendios")
    logging.info("⚙️  Configuración: Historial cada 2s, logs solo con cambios")
    
    while True:
        try:
            current_time = time.time()
            
            # Verificar historial cada 2 segundos
            if current_time - last_history_check >= 2:
                process_history_events()
                last_history_check = current_time
            
            # Procesar notificaciones cada 10 segundos
            if int(current_time) % 10 == 0:
                process_notifications()
                
            time.sleep(0.5)  # Pausa para reducir uso de CPU
            
        except KeyboardInterrupt:
            logging.info("🛑 Deteniendo monitor...")
            break
            
        except Exception as e:
            logging.error(f"Error en el loop principal: {str(e)}")
            time.sleep(5)

if __name__ == '__main__':
    main()