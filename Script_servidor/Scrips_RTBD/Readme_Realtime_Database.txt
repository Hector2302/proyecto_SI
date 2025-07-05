{
  "system": {
    // Nodo principal que contiene toda la información del sistema contra incendios

    "sensors": {
      // Datos en tiempo real provenientes de sensores conectados al PIC

      "temperature": {
        "value": 0.0,                // Valor actual de temperatura en grados Celsius (desde LM35)
        "unit": "Celsius",          // Unidad de medida
        "status": "ok"              // Estado del sensor: ok, error, desconectado, etc.
      },

      "co_level": {
        "ppm": 0,                   // Nivel actual de monóxido de carbono en partes por millón (desde MQ-7)
        "status": "ok"             // Estado del sensor de CO
      },

      "flame": {
        "detected": false,         // Si se ha detectado fuego (true = fuego detectado, desde KY-026)
        "intensity": 0,            // Nivel de intensidad del fuego detectado (opcional, según salida analógica)
        "status": "ok"             // Estado del sensor de flama
      },

      "water_flow": {
        "rate_lpm": 0.0,           // Flujo de agua en litros por minuto (desde YF-S201)
        "total_liters": 0.0,       // Total acumulado de litros utilizados para apagar el fuego
        "status": "ok"             // Estado del sensor de flujo
      }
    },

    "actuators": {
      // Estado de los actuadores del sistema (buzzer y bomba de agua)

      "alarm": {
        "active": false,           // Indica si la alarma (buzzer) está activa
        "duration_sec": 0,         // Tiempo que ha sonado la alarma en segundos
        "trigger_count": 0         // Número de veces que la alarma ha sido activada
      },

      "pump": {
        "active": false,           // Indica si la bomba de agua está en funcionamiento
        "runtime_sec": 0           // Tiempo total que ha estado activa la bomba
      }
    },

    "status": {
      // Estado general del sistema

      "mode": "normal",            // Modo actual del sistema: normal, test, emergency
      "emergency_override": false, // Si está activado un modo manual/emergencia desde la app
      "last_update": "2025-05-22T20:00:00Z",  // Última vez que el sistema actualizó Firebase

      "connectivity": {
        "wifi_connected": true,    // Indica si el ESP32 tiene conexión WiFi activa
        "signal_strength_dbm": -60 // Intensidad de señal WiFi en decibelios (entre -30 y -90 típico)
      }
    },

    "commands": {
      // Comandos enviados desde la app móvil para controlar el sistema de forma remota

      "trigger_test": false,       // Activa una prueba de funcionamiento (buzzer y bomba brevemente)
      "shutdown_system": false,    // Detiene todos los componentes del sistema manualmente
      "override_alarm": false,     // Activa o desactiva manualmente la alarma desde la app

      "last_command": {
        "type": "manual_override", // Tipo de comando enviado (manual_override, test, shutdown, etc.)
        "author": "mobile_app",    // Fuente que envió el comando (ej. nombre de usuario o app)
        "timestamp": "2025-05-22T19:55:00Z" // Fecha y hora del último comando recibido
      }
    },

    "logs": {
      // Historial básico de eventos relevantes ocurridos en el sistema

      "events": [
        {
          "type": "flame_detected",  // Tipo de evento (puede ser flame_detected, system_test, error, etc.)
          "timestamp": "2025-05-22T19:50:00Z", // Fecha y hora del evento
          "details": "Flame detected, alarm triggered, pump activated" // Descripción del evento
        },
        {
          "type": "system_test",     // Otro tipo de evento, en este caso una prueba del sistema
          "timestamp": "2025-05-22T18:00:00Z", // Fecha y hora de la prueba
          "details": "Manual test triggered from app" // Detalle de la acción realizada
        }
      ]
    }
  }
}

--------------------
para ver usarios leer: Notas.txt


{
  "rules": {
    "system": {
      "sensors": {
        ".read": "auth != null",                             // La app y cualquier usuario autenticado puede leer sensores
        ".write": "false"                                    // Sólo services accounts fuera de Firebase escriben (no vía auth)
      },
      "actuators": {
        ".read": "auth != null",
        ".write": "false"
      },
      "status": {
        ".read": "auth != null",
        ".write": "false"
      },
      "commands": {
        ".read": "auth != null",
        ".write": "auth.uid === 'qIrC9TYimQQL5hZzeaGN1c1aOav1'"  // Sólo la app puede enviar comandos (tests, override)
      },
      "logs": {
        ".read": "auth != null",
        ".write": "false"
      },
      "config": {
        ".read": "auth.uid === 'qIrC9TYimQQL5hZzeaGN1c1aOav1'", // Sólo la app lee config
        ".write": "auth.uid === 'qIrC9TYimQQL5hZzeaGN1c1aOav1'"// Sólo la app escribe config
      }
    }
  }
}

