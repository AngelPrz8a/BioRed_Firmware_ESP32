/*
  BioRed v4 — firmware unificado (ESP32-S3 WROOM N16R8, Freenove)

  Sensores como módulos     src/sensores/*.h      (se activan en BioRedConfig.h y en el portal)
  Salidas de datos          SalidaAPI (HTTPS + key)  y  SalidaMQTT (broker)
  Portal de configuración   Portal + Red          (WiFi, sensores, API, MQTT, cámara)

  Pines: DHT22 14 · MQ135 3 · MAX9814 19 · tilt 1,42,2,41 · pan 48,21,47,20 · cámara 4-13,15-18
  Plugin necesario: ninguno (el portal va en flash; ya no se usa LittleFS).
*/
#include <Arduino.h>
#include "src/BioRedConfig.h"
#include "src/nucleo/Config.h"
#include "src/nucleo/Red.h"
#include "src/nucleo/Registro.h"
#include "src/portal/Portal.h"
#include "src/salidas/SalidaAPI.h"
#include "src/salidas/SalidaMQTT.h"
#if BIORED_MODULO_CAMARA
#include "src/modulos/Camara.h"
#endif
#if BIORED_MODULO_MOTORES
#include "src/modulos/Motores.h"
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.printf("\n=== BioRed %s ===\n", BIORED_VERSION);
  analogReadResolution(12);

  cfgCargar();        // configuración guardada (wifi, API, MQTT, sensores)
  registroInit();     // arranca los sensores activos (MQ135 calienta sin bloquear)
#if BIORED_MODULO_MOTORES
  motoresInit();
#endif
#if BIORED_MODULO_CAMARA
  camaraInit();
#endif
  redInit();          // STA con lo guardado o punto de acceso de configuración
  portalInit();
  salidaApiInit();
  salidaMqttInit();
}

void loop() {
  portalLoop();
  redLoop();
  registroActualizar();
}
