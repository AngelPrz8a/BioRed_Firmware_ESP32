#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>   // ArduinoJson 6.x
#include <math.h>

// Un valor medido, listo para MQTT y para el portal.
struct Dato { const char* clave; float valor; const char* unidad; };

// Contrato que cumple cada sensor. Para añadir uno nuevo:
//   1. Crear src/sensores/SensorXxx.h con una clase que herede de Sensor.
//   2. Añadir un interruptor en BioRedConfig.h y 3 líneas en Registro.cpp.
class Sensor {
 public:
  virtual ~Sensor() {}
  virtual const char* id() const = 0;          // "dht22": clave en NVS, topic MQTT y sección del JSON
  virtual const char* nombre() const = 0;      // texto para el portal
  virtual int  pinPorDefecto() const = 0;
  virtual void begin(int pin) = 0;             // se llama una vez si el sensor está activo
  virtual void update() = 0;                   // se llama en cada loop(); no debe bloquear
  virtual const char* estado() const = 0;
  virtual int  datos(Dato* out, int max) const = 0;   // valores vigentes (0 si aún no hay)
  virtual void aApi(JsonDocument& doc) const = 0;     // añade su sección al contrato de ingesta
};

// Limita al rango que acepta la API y redondea (NaN pasa al mínimo).
inline double acotar(double x, double mn, double mx, int dec) {
  if (!isfinite(x) || x < mn) x = mn;
  if (x > mx) x = mx;
  const double f = pow(10.0, dec);
  return round(x * f) / f;
}
