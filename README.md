# Firmware modular ESP32 con WiFi

Sketch Arduino para placas ESP32 compatibles con Arduino-ESP32 y WiFi. La
conexión inalámbrica, el portal de configuración y el almacenamiento
persistente constituyen la base. Sensores, envío de datos, cámara y motores
son módulos opcionales. Todos los interruptores de compilación empiezan en
`0`; los sensores también permanecen desactivados en una instalación nueva.

> La cámara y los motores tienen pines definidos para la Freenove
> ESP32-S3-WROOM CAM y no son universales. Los sensores también utilizan GPIO
> predeterminados que podrían requerir cambios según la placa. Se recomienda
> revisar las limitaciones de cada módulo antes de su activación.

## Requisitos

- Arduino IDE 2.x o Arduino CLI y el paquete **esp32 by Espressif Systems**.
- Una placa ESP32 con WiFi y una versión compatible del core Arduino-ESP32.
  En el entorno de desarrollo debe seleccionarse el modelo exacto de placa y,
  si corresponde, la opción de PSRAM.
- Biblioteca **ArduinoJson 6.x** (dependencia base).
- Para cada función opcional, también deben instalarse las bibliotecas
  indicadas. Algunas placas incluyen `Stepper` o `esp_camera.h` con el paquete
  de la placa; si falta el encabezado, se requiere la dependencia indicada.

| Función | Biblioteca adicional |
|---|---|
| Sensor DHT22 | **DHT sensor library** de Adafruit y **Adafruit Unified Sensor** |
| Sensor MQ135 | **MQUnifiedsensor** |
| Micrófono MAX9814 | Ninguna adicional |
| Salida MQTT | **PubSubClient** |
| Salida API HTTPS | Ninguna adicional; se utilizan `HTTPClient` y `WiFiClientSecure` del core |
| Cámara Freenove S3 | **esp32cam** (bitluni) y `esp_camera.h` del core ESP32 |
| Motores paso a paso | **Stepper** |

Con los interruptores en cero solo se requiere ArduinoJson además del core. Las
bibliotecas de módulos desactivados no se incluyen en la compilación. El
firmware inicia normalmente aunque todos los sensores, módulos y salidas estén
desactivados: WiFi y el portal de configuración forman parte del núcleo.

## Apertura y carga del sketch

El archivo principal es `ESP32-WiFi-Modular.ino`. Arduino requiere que el nombre
de la carpeta del sketch coincida con el del archivo `.ino` principal. Al clonar
este repositorio, su contenido debe copiarse a una carpeta llamada
`ESP32-WiFi-Modular` (incluida la carpeta `src`), o bien debe renombrarse la
carpeta antes de abrirla en Arduino IDE.

1. Instalar las bibliotecas requeridas por los módulos que se habilitarán.
2. Abrir `ESP32-WiFi-Modular.ino` y seleccionar la placa y el puerto serie.
3. Compilar y cargar el firmware.
4. Abrir el monitor serie a **115200 baudios** para consultar el estado y la
   dirección IP.

## Primer arranque y conexión

1. Sin credenciales WiFi guardadas, el ESP32 crea el punto de acceso
   **ESP32-Setup**. La contraseña inicial se define mediante `BIORED_AP_PASS`
   en `src/BioRedConfig.h` (inicialmente `esp32setup`). Se recomienda cambiarla
   antes del despliegue.
2. Al conectarse a esa red y abrir `http://192.168.4.1`, el portal permite
   buscar redes o introducir el SSID y la contraseña de la red WiFi. Al guardar
   los datos, la placa se reinicia.
3. Una vez conectada como estación, la dirección IP puede consultarse en el
   monitor serie o en el router; el portal queda disponible en `http://<IP>`.
   También se anuncia el hostname configurado mediante mDNS, normalmente
   `http://biored.local`.
4. Inicialmente, el portal no tiene contraseña. Se recomienda configurar una
   desde **Acceso al portal** antes de conectarlo a una red compartida.

Si no se establece la conexión, la placa vuelve al punto de acceso de
configuración. No se requiere LittleFS: el portal está integrado en el firmware
y la configuración se guarda en NVS.

## Funcionamiento

- `ESP32-WiFi-Modular.ino` inicia la configuración, los módulos seleccionados,
  la red, el portal y las salidas.
- Si existen credenciales, se intenta conectar a WiFi durante 15 segundos. Si
  la conexión falla, se inicia el punto de acceso de configuración y se vuelve
  a intentar la red guardada periódicamente.
- El portal permite configurar WiFi, activar sensores compilados, cambiar GPIO,
  proteger el portal y configurar API/MQTT. Los cambios de WiFi y sensores
  reinician la placa.
- Cada sensor implementa la interfaz de `src/sensores/Sensor.h`; el registro
  consulta los módulos compilados y actualiza los activos en el ciclo principal.
- API y MQTT solo envían datos si están compilados, habilitados en el portal,
  configurados y conectados a WiFi.
- Las preferencias permanecen en NVS entre reinicios. Un cambio de opción de
  compilación no borra automáticamente preferencias guardadas anteriormente.

## Activación y desactivación de módulos

Las opciones se definen en `src/BioRedConfig.h`: `0` desactiva un módulo durante
la compilación y `1` lo incorpora. A continuación, deben instalarse las
bibliotecas requeridas, compilar y volver a cargar el firmware. Los `#ifndef`
también permiten sobrescribir estas opciones desde el sistema de compilación.

| Opción | Uso y activación |
|---|---|
| `BIORED_SENSOR_DHT22` | DHT22. Asignar `1`, instalar DHT de Adafruit y Adafruit Unified Sensor, conectar el sensor y habilitarlo en **Sensores**. |
| `BIORED_SENSOR_MQ135` | MQ135. Asignar `1`, instalar MQUnifiedsensor, conectar el módulo con la protección de voltaje indicada abajo y habilitarlo en **Sensores**. Requiere calentamiento y calibración. |
| `BIORED_SENSOR_MICROFONO` | MAX9814. Asignar `1`, conectar la salida a un GPIO ADC compatible y habilitarlo en **Sensores**. La disponibilidad del ADC con WiFi depende de la placa. |
| `BIORED_SALIDA_API` | Envío HTTPS. Asignar `1`; en **Conexión** se deben indicar una URL compatible con el formato de datos, la clave API y el intervalo, además de habilitar **Enviar lecturas**. La URL predeterminada está vacía; no hay servidor preconfigurado. |
| `BIORED_SALIDA_MQTT` | MQTT. Asignar `1` e instalar PubSubClient; en **Conexión** se deben configurar servidor, puerto, credenciales (si se utilizan), raíz, nombre de estación e intervalo, además de habilitar **Publicar lecturas**. |
| `BIORED_MODULO_CAMARA` | Cámara. Asignar `1` solo con hardware compatible. La asignación de pines incluida es específica de Freenove ESP32-S3-WROOM CAM y requiere PSRAM configurada. |
| `BIORED_MODULO_MOTORES` | Motores paso a paso. Asignar `1` e instalar Stepper. Los GPIO incluidos son específicos para el montaje S3 indicado en `src/modulos/Motores.cpp`; deben verificarse y, si es necesario, reasignarse. También debe utilizarse un driver adecuado para los motores. |

Para desactivar una función, se debe asignar `0` a su opción y volver a cargar el
firmware. DHT22, MQ135 y MAX9814 también disponen de un control en el portal:
una vez compilado el sensor, debe habilitarse allí y reiniciarse la placa. API y
MQTT requieren asimismo la activación de su casilla y una configuración válida.
La cámara y los motores son módulos de hardware opcionales; su compatibilidad
con otras placas ESP32 no está garantizada.

## Conexión de sensores

Se debe utilizar una fuente adecuada, masa común y el pinout indicado en la hoja
de datos de cada módulo. No se deben conectar señales de 5 V directamente a un
GPIO ESP32: normalmente estos pines solo toleran hasta 3,3 V. En el manual de la
placa se debe comprobar qué GPIO están disponibles, cuáles son ADC y cuáles
están reservados. Los pines siguientes son valores iniciales editables desde
**Sensores** después de compilar el módulo; no son universales.

| Módulo | Conexión inicial | Precauciones |
|---|---|---|
| DHT22 | VCC y GND según el módulo; DATA a GPIO 14. | En un sensor sin placa adaptadora suele requerirse una resistencia pull-up de 4,7–10 kΩ entre DATA y VCC. |
| MQ135 | Alimentación según el módulo; salida analógica a GPIO 3 mediante un divisor apropiado. | El calentador suele requerir 5 V y la salida puede superar 3,3 V. El divisor debe dimensionarse según el voltaje máximo del ADC de la placa; el código utiliza valores de ADC orientados a ESP32 de 12 bits. |
| MAX9814 | VCC/GND según el módulo; OUT a GPIO 19 o a un ADC compatible de la placa. | ADC2 puede entrar en conflicto con WiFi en algunas placas ESP32. Se recomienda seleccionar un ADC que el fabricante indique como compatible con WiFi y ajustar el pin. |

El portal permite cambiar el GPIO de los sensores compilados, pero el rango de
entrada no garantiza que ese pin exista o sea seguro en el modelo utilizado. El
pinout debe verificarse antes de guardar.

## Personalización y notas

- Para personalizar el punto de acceso y el nombre mDNS, se pueden modificar
  `BIORED_AP_SSID`, `BIORED_AP_PASS` y `BIORED_HOSTNAME` en
  `src/BioRedConfig.h`.
- `BIORED_VERSION` se muestra en el monitor serie y en los datos publicados.
- La salida HTTPS construye un JSON con UID del dispositivo, tiempo activo y
  lecturas de sensores. El servidor debe aceptar ese formato y una cabecera
  `x-api-key`; para el servidor elegido debe configurarse un certificado raíz
  válido en `src/salidas/Certificados.h`.
- MQTT publica JSON en `<raíz>/<estación>/<sensor>` y un topic de estado en
  `<raíz>/<estación>/estado`.
- Los valores GPIO predeterminados de cada sensor se definen en su clase
  `src/sensores/Sensor*.h`; los pines de cámara y motores se definen en sus
  respectivos archivos de módulo.
