#pragma once
#include <Arduino.h>

// WiFi: se conecta con las credenciales guardadas; si no hay o fallan,
// abre el punto de acceso de configuración con portal cautivo.
void   redInit();
void   redLoop();
bool   redConectada();
bool   redModoAP();
String redIP();
String redRedesJson();   // {"escaneando":bool,"redes":[{ssid,rssi}]} (escaneo asíncrono)
