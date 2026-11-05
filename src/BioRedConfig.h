#pragma once
// ═══════════════════════════════════════════════════════════════
//  Interruptores de compilación de BioRed.
//  Pon 0 en un módulo para quitarlo: no se compila y su librería
//  de Arduino deja de ser necesaria.
// ═══════════════════════════════════════════════════════════════
#define BIORED_VERSION "4.0.0"

// Sensores (cada uno es un archivo en src/sensores/)
#ifndef BIORED_SENSOR_DHT22
#define BIORED_SENSOR_DHT22      1
#endif
#ifndef BIORED_SENSOR_MQ135
#define BIORED_SENSOR_MQ135      0
#endif
#ifndef BIORED_SENSOR_MICROFONO
#define BIORED_SENSOR_MICROFONO  0
#endif

// Salidas de datos
#ifndef BIORED_SALIDA_API
#define BIORED_SALIDA_API        0   // HTTPS + key de dispositivo
#endif
#ifndef BIORED_SALIDA_MQTT
#define BIORED_SALIDA_MQTT       1   // broker MQTT
#endif

// Módulos opcionales (no son sensores)
#ifndef BIORED_MODULO_CAMARA
#define BIORED_MODULO_CAMARA     0
#endif
#ifndef BIORED_MODULO_MOTORES
#define BIORED_MODULO_MOTORES    0
#endif

// Valores de fábrica (luego se cambian desde el portal)
#define BIORED_API_URL_DEFECTO   "https://biored-analytic.brayan36.com/api/v1/readings/ingest"
#define BIORED_AP_SSID           "BioRed-Setup"
#define BIORED_AP_PASS           "biored1234"   // mínimo 8 caracteres
#define BIORED_HOSTNAME          "biored"       // http://biored.local
