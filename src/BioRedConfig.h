#pragma once
// Interruptores de compilación: 0 desactiva y 1 incluye el módulo.
// Los módulos opcionales se compilan desactivados por defecto.
#define BIORED_VERSION "5.2.0"

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
#define BIORED_SALIDA_MQTT       0   // broker MQTT
#endif

// Módulos opcionales (no son sensores)
#ifndef BIORED_MODULO_CAMARA
#define BIORED_MODULO_CAMARA     0
#endif
#ifndef BIORED_MODULO_MOTORES
#define BIORED_MODULO_MOTORES    0
#endif

// Valores de fábrica (luego se cambian desde el portal)
#define BIORED_API_URL_DEFECTO   ""
#define BIORED_AP_SSID           "ESP32-Setup"
#define BIORED_AP_PASS           "esp32setup"   // cámbiese; mínimo 8 caracteres
#define BIORED_HOSTNAME          "biored" // http://biored.local
