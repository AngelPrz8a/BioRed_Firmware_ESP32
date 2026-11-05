#include "Registro.h"
#include "../BioRedConfig.h"
#include "Config.h"

#if BIORED_SENSOR_DHT22
#include "../sensores/SensorDHT22.h"
#endif
#if BIORED_SENSOR_MQ135
#include "../sensores/SensorMQ135.h"
#endif
#if BIORED_SENSOR_MICROFONO
#include "../sensores/SensorMicrofono.h"
#endif

namespace {
  struct EntradaSensor {
    Sensor* sensor;
    bool activo;
    int pin;
  };

  EntradaSensor sensoresRegistrados[8];
  int cantidadSensores = 0;
  SemaphoreHandle_t mutexRegistro = nullptr;

  struct BloqueoRegistro {
    bool adquirido;

    BloqueoRegistro() {
      adquirido = xSemaphoreTake(mutexRegistro, pdMS_TO_TICKS(50)) == pdTRUE;
    }

    ~BloqueoRegistro() {
      if (adquirido) xSemaphoreGive(mutexRegistro);
    }
  };

  void registrarSensor(Sensor* sensor) {
    if (cantidadSensores >= 8) return;

    const bool activo = sensorHabilitadoEnConfiguracion(sensor->identificador(), false);
    const int pin = pinSensorEnConfiguracion(sensor->identificador(), sensor->pinPredeterminado());
    if (activo) {
      sensor->iniciar(pin);
      Serial.printf("[Registro] %s activo en GPIO%d\n", sensor->identificador(), pin);
    }

    sensoresRegistrados[cantidadSensores++] = {sensor, activo, pin};
  }
}

void iniciarRegistroSensores() {
  mutexRegistro = xSemaphoreCreateMutex();
  // Cada sensor requiere su interruptor en BioRedConfig.h y habilitarse en el portal.
#if BIORED_SENSOR_DHT22
  static SensorDHT22 dht;
  registrarSensor(&dht);
#endif
#if BIORED_SENSOR_MQ135
  static SensorMQ135 mq;
  registrarSensor(&mq);
#endif
#if BIORED_SENSOR_MICROFONO
  static SensorMicrofono microfono;
  registrarSensor(&microfono);
#endif
}

void actualizarRegistroSensores() {
  for (int indiceSensor = 0; indiceSensor < cantidadSensores; indiceSensor++) {
    if (!sensoresRegistrados[indiceSensor].activo) continue;

    BloqueoRegistro bloqueo;
    if (bloqueo.adquirido) sensoresRegistrados[indiceSensor].sensor->actualizar();
  }
}

int cantidadSensoresRegistrados() {
  return cantidadSensores;
}

Sensor* obtenerSensorRegistrado(int indice) {
  return sensoresRegistrados[indice].sensor;
}

bool sensorRegistradoActivo(int indice) {
  return sensoresRegistrados[indice].activo;
}

int pinSensorRegistrado(int indice) {
  return sensoresRegistrados[indice].pin;
}

void configurarSensorRegistrado(const char* identificadorSensor, bool activo, int pin) {
  for (int indiceSensor = 0; indiceSensor < cantidadSensores; indiceSensor++) {
    Sensor* sensor = sensoresRegistrados[indiceSensor].sensor;
    if (strcmp(sensor->identificador(), identificadorSensor) != 0) continue;

    if (pin < 0 || pin > 48) pin = sensor->pinPredeterminado();
    guardarConfiguracionDelSensor(identificadorSensor, activo, pin);
  }
}

int obtenerLecturasSensor(int indiceSensor, LecturaSensor* datos, int capacidad) {
  if (indiceSensor < 0 || indiceSensor >= cantidadSensores ||
      !sensoresRegistrados[indiceSensor].activo) {
    return 0;
  }

  BloqueoRegistro bloqueo;
  return bloqueo.adquirido
      ? sensoresRegistrados[indiceSensor].sensor->obtenerLecturas(datos, capacidad)
      : 0;
}

bool agregarSensoresAlJsonApi(JsonDocument& doc) {
  BloqueoRegistro bloqueo;
  if (!bloqueo.adquirido) return false;

  for (int indiceSensor = 0; indiceSensor < cantidadSensores; indiceSensor++) {
    if (sensoresRegistrados[indiceSensor].activo) {
      sensoresRegistrados[indiceSensor].sensor->agregarLecturasAlJsonApi(doc);
    }
  }

  return true;
}
