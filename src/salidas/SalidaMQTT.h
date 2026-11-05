#pragma once
#include <Arduino.h>
#include "../BioRedConfig.h"

// Publica cada sensor activo en <raiz>/<módulo>/<id_sensor> como JSON, p. ej.
//   biored/sensor-1/dht22  {"temperatura":22.6,"humedad":61.8}
// y el estado del dispositivo (retenido, con testamento) en <raiz>/<módulo>/estado.
// Corre en una tarea propia: un broker caído no bloquea el resto del firmware.
struct EstadoMqtt { bool conectado; uint32_t publicados; char error[64]; };

#if BIORED_SALIDA_MQTT
void salidaMqttInit();
void salidaMqttAplicar();                // reconecta con la configuración nueva
const EstadoMqtt& salidaMqttEstado();
#else
inline void salidaMqttInit() {}
inline void salidaMqttAplicar() {}
inline const EstadoMqtt& salidaMqttEstado() { static EstadoMqtt e = {false, 0, ""}; return e; }
#endif
