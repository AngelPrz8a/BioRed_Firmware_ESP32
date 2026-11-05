#pragma once
#include <Arduino.h>

// WiFi: se conecta con las credenciales guardadas; si no hay o fallan,
// abre el punto de acceso de configuración con portal cautivo.
void   iniciarRed();
void   actualizarRed();
bool   wifiConectado();
bool   modoPuntoAcceso();
String obtenerDireccionIp();
String obtenerRedesComoJson();   // {"escaneando":bool,"redes":[{ssid,rssi}]} (escaneo asíncrono)
