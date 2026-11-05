/*
  Firmware modular para placas ESP32 con WiFi (Arduino-ESP32).

  Sensores como módulos     src/sensores/*.h      (se activan en BioRedConfig.h y en el portal)
  Salidas de datos          API HTTPS y MQTT      (opcionales)
  Portal de configuración   Portal + WiFi         (credenciales, sensores y salidas)

  Los GPIO predeterminados y los módulos de cámara/motores dependen de la placa.
  Se recomienda consultar README.md antes de conectar o habilitar módulos.
  Plugin necesario: ninguno (el portal está integrado; no se usa LittleFS).
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
  Serial.printf("\n=== ESP32 WiFi Modular %s ===\n", BIORED_VERSION);
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
