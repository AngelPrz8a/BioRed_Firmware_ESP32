#include "Config.h"
#include <Preferences.h>
#include "../BioRedConfig.h"

Configuracion configuracion;

namespace {
  const char NOMBRE_ESPACIO_NVS[] = "biored";

  void leerTexto(Preferences& preferencias, const char* clave, char* destino,
                 size_t capacidad, const char* valorPredeterminado) {
    strlcpy(destino, preferencias.getString(clave, valorPredeterminado).c_str(), capacidad);
  }
}

void cargarConfiguracion() {
  Preferences preferencias;
  preferencias.begin(NOMBRE_ESPACIO_NVS, false);

  leerTexto(preferencias, "ssid", configuracion.nombreRedWifi, sizeof configuracion.nombreRedWifi, "");
  leerTexto(preferencias, "wpass", configuracion.contrasenaRedWifi, sizeof configuracion.contrasenaRedWifi, "");

  configuracion.apiHabilitada = preferencias.getBool("api_on", false);
  leerTexto(preferencias, "api_url", configuracion.urlApi, sizeof configuracion.urlApi, BIORED_API_URL_DEFECTO);
  leerTexto(preferencias, "api_key", configuracion.claveApi, sizeof configuracion.claveApi, "");
  leerTexto(preferencias, "uid", configuracion.identificadorDispositivo, sizeof configuracion.identificadorDispositivo, "");
  configuracion.intervaloApiSegundos = preferencias.getUInt("api_int", 30);

  configuracion.mqttHabilitado = preferencias.getBool("mq_on", false);
  leerTexto(preferencias, "mq_host", configuracion.servidorMqtt, sizeof configuracion.servidorMqtt, "");
  leerTexto(preferencias, "mq_user", configuracion.usuarioMqtt, sizeof configuracion.usuarioMqtt, "");
  leerTexto(preferencias, "mq_pass", configuracion.contrasenaMqtt, sizeof configuracion.contrasenaMqtt, "");
  leerTexto(preferencias, "mq_raiz", configuracion.raizTopicsMqtt, sizeof configuracion.raizTopicsMqtt, "sensores");
  leerTexto(preferencias, "mq_mod", configuracion.nombreEstacionMqtt, sizeof configuracion.nombreEstacionMqtt, "sensor-1");
  configuracion.puertoMqtt = preferencias.getUShort("mq_port", 1883);
  configuracion.intervaloMqttSegundos = preferencias.getUInt("mq_int", 10);

  leerTexto(preferencias, "portal", configuracion.contrasenaPortal, sizeof configuracion.contrasenaPortal, "");
  preferencias.end();
}

void guardarConfiguracion() {
  Preferences preferencias;
  preferencias.begin(NOMBRE_ESPACIO_NVS, false);

  preferencias.putString("ssid", configuracion.nombreRedWifi);
  preferencias.putString("wpass", configuracion.contrasenaRedWifi);

  preferencias.putBool("api_on", configuracion.apiHabilitada);
  preferencias.putString("api_url", configuracion.urlApi);
  preferencias.putString("api_key", configuracion.claveApi);
  preferencias.putString("uid", configuracion.identificadorDispositivo);
  preferencias.putUInt("api_int", configuracion.intervaloApiSegundos);

  preferencias.putBool("mq_on", configuracion.mqttHabilitado);
  preferencias.putString("mq_host", configuracion.servidorMqtt);
  preferencias.putString("mq_user", configuracion.usuarioMqtt);
  preferencias.putString("mq_pass", configuracion.contrasenaMqtt);
  preferencias.putString("mq_raiz", configuracion.raizTopicsMqtt);
  preferencias.putString("mq_mod", configuracion.nombreEstacionMqtt);
  preferencias.putUShort("mq_port", configuracion.puertoMqtt);
  preferencias.putUInt("mq_int", configuracion.intervaloMqttSegundos);

  preferencias.putString("portal", configuracion.contrasenaPortal);
  preferencias.end();
}

bool sensorHabilitadoEnConfiguracion(const char* identificadorSensor, bool valorPredeterminado) {
  Preferences preferencias;
  preferencias.begin(NOMBRE_ESPACIO_NVS, false);
  const bool habilitado = preferencias.getBool(
      (String(identificadorSensor) + "_on").c_str(), valorPredeterminado);
  preferencias.end();
  return habilitado;
}

int pinSensorEnConfiguracion(const char* identificadorSensor, int pinPredeterminado) {
  Preferences preferencias;
  preferencias.begin(NOMBRE_ESPACIO_NVS, false);
  const int pin = preferencias.getInt(
      (String(identificadorSensor) + "_pin").c_str(), pinPredeterminado);
  preferencias.end();
  return pin;
}

void guardarConfiguracionDelSensor(const char* identificadorSensor, bool habilitado, int pin) {
  Preferences preferencias;
  preferencias.begin(NOMBRE_ESPACIO_NVS, false);
  preferencias.putBool((String(identificadorSensor) + "_on").c_str(), habilitado);
  preferencias.putInt((String(identificadorSensor) + "_pin").c_str(), pin);
  preferencias.end();
}
