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
  struct Entrada { Sensor* s; bool activo; int pin; };
  Entrada lista[8];
  int n = 0;
  SemaphoreHandle_t mtx = nullptr;

  struct Cerrojo {
    bool ok;
    Cerrojo()  { ok = xSemaphoreTake(mtx, pdMS_TO_TICKS(50)) == pdTRUE; }
    ~Cerrojo() { if (ok) xSemaphoreGive(mtx); }
  };

  void alta(Sensor* s) {
    if (n >= 8) return;
    const bool on = cfgSensorOn(s->id(), false);
    const int pin = cfgSensorPin(s->id(), s->pinPorDefecto());
    if (on) { s->begin(pin); Serial.printf("[Registro] %s activo en GPIO%d\n", s->id(), pin); }
    lista[n++] = {s, on, pin};
  }
}

void registroInit() {
  mtx = xSemaphoreCreateMutex();
  // Cada sensor requiere su interruptor en BioRedConfig.h y habilitarse en el portal.
#if BIORED_SENSOR_DHT22
  static SensorDHT22 dht;        alta(&dht);
#endif
#if BIORED_SENSOR_MQ135
  static SensorMQ135 mq;         alta(&mq);
#endif
#if BIORED_SENSOR_MICROFONO
  static SensorMicrofono mic;    alta(&mic);
#endif
}

void registroActualizar() {
  for (int i = 0; i < n; i++) {
    if (!lista[i].activo) continue;
    Cerrojo c;
    if (c.ok) lista[i].s->update();
  }
}

int     registroCantidad()      { return n; }
Sensor* registroSensor(int i)   { return lista[i].s; }
bool    registroActivo(int i)   { return lista[i].activo; }
int     registroPin(int i)      { return lista[i].pin; }

void registroConfigurar(const char* id, bool on, int pin) {
  for (int i = 0; i < n; i++) {
    if (strcmp(lista[i].s->id(), id) != 0) continue;
    if (pin < 0 || pin > 48) pin = lista[i].s->pinPorDefecto();
    cfgSensorGuardar(id, on, pin);
  }
}

int registroDatos(int i, Dato* out, int max) {
  if (i < 0 || i >= n || !lista[i].activo) return 0;
  Cerrojo c;
  return c.ok ? lista[i].s->datos(out, max) : 0;
}

bool registroArmarApi(JsonDocument& doc) {
  Cerrojo c;
  if (!c.ok) return false;
  for (int i = 0; i < n; i++) if (lista[i].activo) lista[i].s->aApi(doc);
  return true;
}
