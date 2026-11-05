#pragma once
#include <Arduino.h>
#include "../BioRedConfig.h"

// Envío HTTPS a una API configurada por el usuario, con credencial opcional.
// Corre en su propia tarea (núcleo 0) para que el
// handshake TLS no detenga el portal, la cámara ni el micrófono.
struct EstadoEnvioApi { uint32_t enviados; uint32_t fallidos; int ultimoCodigo; };

#if BIORED_SALIDA_API
void iniciarEnvioApi();                 // después de iniciarRed()
const EstadoEnvioApi& obtenerEstadoEnvioApi();
#else
inline void iniciarEnvioApi() {}
inline const EstadoEnvioApi& obtenerEstadoEnvioApi() { static EstadoEnvioApi e = {0, 0, 0}; return e; }
#endif
