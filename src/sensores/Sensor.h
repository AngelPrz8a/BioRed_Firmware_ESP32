#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>   // ArduinoJson 6.x
#include <math.h>

// Una lectura de sensor con su clave, valor y unidad.
struct LecturaSensor { const char* clave; float valor; const char* unidad; };

// Contrato común que implementa cada sensor incluido en el firmware.
// Para añadir uno, implementa estos métodos y regístralo en Registro.cpp.
class Sensor {
 public:
  virtual ~Sensor() {}
  virtual const char* identificador() const = 0;  // Identificador estable en NVS, MQTT y la API.
  virtual const char* nombreParaMostrar() const = 0;
  virtual int pinPredeterminado() const = 0;
  virtual void iniciar(int pin) = 0;
  virtual void actualizar() = 0;  // Se llama en cada loop(); no debe bloquear.
  virtual const char* estadoActual() const = 0;
  virtual int obtenerLecturas(LecturaSensor* lecturas, int capacidad) const = 0;
  virtual void agregarLecturasAlJsonApi(JsonDocument& documento) const = 0;
};

// Limita al rango que acepta la API y redondea (NaN pasa al mínimo).
inline double limitarYRedondear(double valor, double minimo, double maximo, int decimales) {
  if (!isfinite(valor) || valor < minimo) valor = minimo;
  if (valor > maximo) valor = maximo;
  const double factorRedondeo = pow(10.0, decimales);
  return round(valor * factorRedondeo) / factorRedondeo;
}
