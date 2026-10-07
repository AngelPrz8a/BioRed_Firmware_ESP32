# BioRed Firmware ESP32

Firmware para ESP32 WROOM-32 con portal web de configuración Wi-Fi, monitoreo de un sensor DHT y publicación de mensajes MQTT.

## Funcionalidades

- Crea un punto de acceso para configurar el dispositivo y mantiene el modo estación para conectarse a una red Wi-Fi.
- Guarda las credenciales Wi-Fi y la configuración en la memoria persistente `Preferences` del ESP32.
- Permite configurar un sensor DHT11 o DHT22, seleccionar su GPIO y probar la conexión.
- Muestra el estado de Internet, MQTT y los sensores, además del uso de memoria del ESP32.
- Permite configurar un broker MQTT, un topic, tags y un payload JSON.
- Reintenta la conexión MQTT y publica cada 5 segundos mientras el servicio MQTT esté habilitado.

## Hardware

- ESP32 WROOM-32 (placa sugerida: **ESP32 Dev Module**).
- Sensor DHT11 o DHT22, si se utilizará la lectura de temperatura y humedad.
- Conexión a una red Wi-Fi de 2,4 GHz para acceder a Internet y al broker MQTT.

El portal ofrece estos GPIO para asignar servicios: `4, 5, 12, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33`. Verifica las restricciones de arranque y el cableado de tu placa antes de elegir un pin.

## Requisitos

Instala Arduino IDE y el soporte de placas **ESP32 by Espressif Systems**. Instala también estas bibliotecas desde el gestor de Arduino:

- **DHT sensor library** de Adafruit.
- **Adafruit Unified Sensor**.
- **PubSubClient** de Nick O'Leary.

`WiFi`, `WebServer`, `DNSServer` y `Preferences` forman parte del core de ESP32.

## Compilar y cargar

1. Abre `firmware.ino` en Arduino IDE. Si Arduino solicita un sketch con nombre de carpeta coincidente, guarda el sketch como `firmware/firmware.ino`.
2. Selecciona **ESP32 Dev Module** y el puerto serial correspondiente.
3. Compila y carga el firmware.
4. Abre el monitor serial a `115200` baudios para ver el estado de arranque y la dirección IP del portal.

## Primer inicio

1. Conéctate al punto de acceso `ESP32-Setup`.
2. Abre en el navegador la dirección IP que aparece en el monitor serial; normalmente el ESP32 muestra la IP de su punto de acceso al iniciar.
3. En **Red**, selecciona tu Wi-Fi, introduce la contraseña y conecta el dispositivo.
4. En **Sensores**, habilita el servicio DHT, selecciona el GPIO y el modelo, y guarda la configuración. Usa **Probar conexión** para comprobar el sensor.
5. En **Conexiones**, habilita MQTT e introduce el host y puerto del broker (por defecto `1883`). Guarda para conectar.
6. En **Sensores**, habilita el servicio MQTT y configura el topic, los tags y el payload; después guarda los cambios.

El punto de acceso está configurado en el código como `ESP32-Setup` y sin contraseña. La dirección y contraseña del Wi-Fi de estación se guardan en `Preferences` y se vuelven a intentar al iniciar.

## Páginas del portal

| Ruta | Uso |
| --- | --- |
| `/` | Estado general de Internet, MQTT y sensores. |
| `/red` | Selección y conexión a una red Wi-Fi. |
| `/sensores` | Habilitar servicios, asignar GPIO, elegir modelo DHT, probar y guardar. |
| `/conexiones` | Configurar y habilitar el broker MQTT. |
| `/memoria` | Consultar métricas de memoria RAM y tiempo encendido. |

## MQTT

El broker se configura por separado del servicio MQTT. Para publicar, deben estar habilitados MQTT en **Conexiones** y el servicio MQTT en **Sensores**; el servicio también necesita un topic no vacío. El firmware publica cada cinco segundos.

Si **Tags** está vacío, se envía el contenido de **Payload** directamente. Si contiene tags separados por comas, el firmware los incluye en un objeto JSON con esta forma:

```json
{"service":"mqtt","tags":["telemetria"],"payload":{"value":"ok"}}
```

En ese caso, **Payload** debe ser JSON válido porque se inserta como valor JSON, no como texto escapado. El valor inicial es `{"service":"mqtt","value":"ok"}`.

## Seguridad y limitaciones

- El punto de acceso de configuración no tiene contraseña por defecto. Configura `AP_PASSWORD` en `firmware.ino` antes de instalar el dispositivo en un entorno no confiable; la contraseña de un AP protegido debe tener al menos ocho caracteres.
- MQTT utiliza una conexión TCP sin credenciales de broker ni TLS en la implementación actual. Úsalo solo en redes de confianza o añade autenticación y TLS antes de exponerlo a redes no confiables.
- El firmware admite actualmente los servicios DHT y MQTT definidos en `inicializarServiciosDisponibles()`; agregar una entrada al registro no implementa automáticamente su lógica de lectura o publicación.
- La compilación confirma el código, pero la conexión Wi-Fi, el sensor y el broker requieren validación en el hardware y la red reales.
