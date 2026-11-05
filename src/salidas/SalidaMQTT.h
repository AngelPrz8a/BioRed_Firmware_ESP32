#pragma once
#include <Arduino.h>
#include "../BioRedConfig.h"

// Publica cada sensor activo en <raiz>/<módulo>/<id_sensor> como JSON, p. ej.
//   sensores/nodo-1/dht22  {"temperatura":22.6,"humedad":61.8}
// y el estado del dispositivo (retenido, con testamento) en <raiz>/<módulo>/estado.
// Corre en una tarea propia: un broker caído no bloquea el resto del firmware.
struct EstadoConexionMqtt { bool conectado; uint32_t publicados; char error[64]; };

#if BIORED_SALIDA_MQTT
void iniciarPublicacionMqtt();
void reconectarMqtt();                // reconecta con la configuración nueva
const EstadoConexionMqtt& obtenerEstadoMqtt();
#else
inline void iniciarPublicacionMqtt() {}
inline void reconectarMqtt() {}
inline const EstadoConexionMqtt& obtenerEstadoMqtt() { static EstadoConexionMqtt e = {false, 0, ""}; return e; }
#endif
