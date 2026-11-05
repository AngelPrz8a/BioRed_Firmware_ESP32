#pragma once
#include <Arduino.h>

// Configuración persistente del dispositivo, editada desde el portal.
struct Configuracion {
  char nombreRedWifi[33];
  char contrasenaRedWifi[65];

  bool     apiHabilitada;
  char     urlApi[160];
  char     claveApi[96];
  char     identificadorDispositivo[18];
  uint32_t intervaloApiSegundos;

  bool     mqttHabilitado;
  char     servidorMqtt[64];
  char     usuarioMqtt[32];
  char     contrasenaMqtt[48];
  char     raizTopicsMqtt[24];
  char     nombreEstacionMqtt[24];
  uint16_t puertoMqtt;
  uint32_t intervaloMqttSegundos;

  char contrasenaPortal[32];
};
extern Configuracion configuracion;

void cargarConfiguracion();
void guardarConfiguracion();

// Preferencias de cada sensor: "<identificador>_on" y "<identificador>_pin".
bool sensorHabilitadoEnConfiguracion(const char* identificadorSensor, bool valorPredeterminado);
int  pinSensorEnConfiguracion(const char* identificadorSensor, int pinPredeterminado);
void guardarConfiguracionDelSensor(const char* identificadorSensor, bool habilitado, int pin);
