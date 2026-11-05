#pragma once
#include "Sensor.h"
#include <DHT.h>   // Adafruit DHT sensor library

class SensorDHT22 : public Sensor {
  DHT*     dht_ = nullptr;
  float    t_ = NAN, h_ = NAN;
  bool     ok_ = false;
  uint32_t ultimo_ = 0;
  static const uint32_t INTERVALO_MS = 3000;

 public:
  const char* identificador() const override     { return "dht22"; }
  const char* nombreParaMostrar() const override { return "DHT22 (temperatura y humedad)"; }
  int pinPredeterminado() const override  { return 14; }

  void iniciar(int pin) override {
    dht_ = new DHT(pin, DHT22);
    dht_->begin();
    ultimo_ = millis() - INTERVALO_MS;   // primera lectura inmediata
  }

  void actualizar() override {
    if (!dht_ || millis() - ultimo_ < INTERVALO_MS) return;
    ultimo_ = millis();
    const float h = dht_->readHumidity(), t = dht_->readTemperature();
    ok_ = !isnan(h) && !isnan(t) && t >= -40 && t <= 80 && h >= 0 && h <= 100;
    if (ok_) { t_ = t; h_ = h; }
  }

  const char* estadoActual() const override { return ok_ ? "ok" : "sin datos"; }

  int obtenerLecturas(LecturaSensor* out, int max) const override {
    if (!ok_ || max < 2) return 0;
    out[0] = {"temperatura", t_, "°C"};
    out[1] = {"humedad",     h_, "%"};
    return 2;
  }

  void agregarLecturasAlJsonApi(JsonDocument& doc) const override {
    JsonObject o = doc.createNestedObject("dht22");
    o["valid"] = ok_;
    if (ok_) {
      o["temperature_c"] = limitarYRedondear(t_, -40, 80, 1);
      o["humidity_pct"]  = limitarYRedondear(h_, 0, 100, 1);
    } else {
      o["temperature_c"] = nullptr;
      o["humidity_pct"]  = nullptr;
    }
  }
};
