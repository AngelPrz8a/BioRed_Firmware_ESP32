#pragma once
#include <Arduino.h>

// Configuración persistente (NVS). Se edita desde el portal.
struct Cfg {
  char wifiSsid[33], wifiPass[65];
  // API BioRed (HTTPS + key de dispositivo)
  bool     apiOn;
  char     apiUrl[160], apiKey[96], deviceUid[18];
  uint32_t apiIntervaloS;
  // MQTT
  bool     mqttOn;
  char     mqttHost[64], mqttUser[32], mqttPass[48], mqttRaiz[24], mqttModulo[24];
  uint16_t mqttPort;
  uint32_t mqttIntervaloS;
  // Portal
  char portalPass[32];     // vacío = sin clave. Usuario: admin
};
extern Cfg cfg;

void cfgCargar();
void cfgGuardar();

// Configuración por sensor (clave "<id>_on" y "<id>_pin")
bool cfgSensorOn(const char* id, bool defecto);
int  cfgSensorPin(const char* id, int defecto);
void cfgSensorGuardar(const char* id, bool on, int pin);
