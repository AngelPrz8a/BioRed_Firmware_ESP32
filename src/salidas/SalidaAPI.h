#pragma once
#include <Arduino.h>
#include "../BioRedConfig.h"

// Envío HTTPS a una API configurada por el usuario, con credencial opcional.
// Corre en su propia tarea (núcleo 0) para que el
// handshake TLS no detenga el portal, la cámara ni el micrófono.
struct EstadoApi { uint32_t enviados; uint32_t fallidos; int ultimoCodigo; };

#if BIORED_SALIDA_API
void salidaApiInit();                 // después de redInit()
const EstadoApi& salidaApiEstado();
#else
inline void salidaApiInit() {}
inline const EstadoApi& salidaApiEstado() { static EstadoApi e = {0, 0, 0}; return e; }
#endif
