#pragma once
#include "Sensor.h"
#include <MQUnifiedsensor.h>

namespace {
  const float    MQ_VCC = 5.0f, MQ_VREF = 3.3f, MQ_RL_KOHM = 10.0f, MQ_AIRE_LIMPIO = 3.6f;
  const uint32_t MQ_CALENTAMIENTO_MS = 180000UL, MQ_CALIB_MS = 200, MQ_LECTURA_MS = 2000;
  const int      MQ_CALIB_MUESTRAS = 30;
}

// MQ135 sin divisor de tensión: la salida puede acercarse a 5 V y el ADC del
// ESP32-S3 admite 3,3 V. Se recomienda un divisor 10k/15k en despliegues fijos.
class SensorMQ135 : public Sensor {
  enum Fase { CALENTANDO, CALIBRANDO, LISTO };
  MQUnifiedsensor* mq_ = nullptr;
  int      pin_ = 3;
  Fase     fase_ = CALENTANDO;
  uint32_t inicio_ = 0, ultMuestra_ = 0, ultLectura_ = 0;
  int      muestra_ = 0;
  float    r0Acum_ = 0;
  struct { float co2=0, co=0, nh3=0, alcohol=0, tolueno=0, acetona=0, rs=0, vAout=0;
           int adcRaw=0; uint32_t ts=0; } g_;

  float voltaje() const { return analogRead(pin_) * (MQ_VREF / 4095.0f); }
  float gas(float a, float b) { mq_->setA(a); mq_->setB(b); return mq_->readSensor(); }
  static bool ppmOk(float p) { return isfinite(p) && p >= 0.0f && p <= 99999999.99f; }

 public:
  const char* id() const override     { return "mq135"; }
  const char* nombre() const override { return "MQ135 (calidad del aire)"; }
  int pinPorDefecto() const override  { return 3; }

  void begin(int pin) override {
    pin_ = pin;
    mq_ = new MQUnifiedsensor("ESP32", MQ_VCC, 12, pin, "MQ-135");
    fase_ = CALENTANDO;
    inicio_ = millis();
    Serial.println("[MQ135] Calentando 3 min (no bloquea)");
  }

  void update() override {
    if (!mq_) return;
    switch (fase_) {
      case CALENTANDO:
        if (millis() - inicio_ < MQ_CALENTAMIENTO_MS) break;
        mq_->setRegressionMethod(1);
        mq_->init();
        fase_ = CALIBRANDO; muestra_ = 0; r0Acum_ = 0; ultMuestra_ = 0;
        Serial.println("[MQ135] Calibrando R0 (aire limpio)");
        break;

      case CALIBRANDO:
        if (millis() - ultMuestra_ < MQ_CALIB_MS) break;
        ultMuestra_ = millis();
        mq_->externalADCUpdate(voltaje());
        r0Acum_ += mq_->calibrate(MQ_AIRE_LIMPIO);
        if (++muestra_ >= MQ_CALIB_MUESTRAS) {
          const float r0 = r0Acum_ / MQ_CALIB_MUESTRAS;
          mq_->setR0(r0);
          Serial.printf("[MQ135] R0 = %.3f kOhm%s\n", r0,
                        (r0 < 1.0f || r0 > 100.0f) ? "  AVISO fuera de 1-100: revisa alimentacion 5V y aire limpio" : "");
          fase_ = LISTO;
        }
        break;

      case LISTO:
        if (millis() - ultLectura_ < MQ_LECTURA_MS) break;
        ultLectura_ = millis();
        g_.adcRaw = analogRead(pin_);
        g_.vAout  = voltaje();
        g_.rs     = g_.vAout < 0.01f ? 0.0f : MQ_RL_KOHM * ((MQ_VCC - g_.vAout) / g_.vAout);
        mq_->externalADCUpdate(g_.vAout);
        // Curvas A/B del datasheet (20 °C, 65 %HR). Valores orientativos.
        g_.co2     = gas(110.47f, -2.862f);
        g_.co      = gas(605.18f, -3.937f);
        g_.nh3     = gas(102.2f,  -2.473f);
        g_.alcohol = gas(77.255f, -3.18f);
        g_.tolueno = gas(44.947f, -3.445f);
        g_.acetona = gas(34.668f, -3.369f);
        g_.ts = millis();
        break;
    }
  }

  const char* estado() const override {
    return fase_ == CALENTANDO ? "calentando" : fase_ == CALIBRANDO ? "calibrando" : "listo";
  }

  int datos(Dato* out, int max) const override {
    if (fase_ != LISTO || g_.ts == 0) return 0;
    const Dato todos[] = {{"co2", g_.co2, "ppm"}, {"co", g_.co, "ppm"}, {"nh3", g_.nh3, "ppm"},
                          {"alcohol", g_.alcohol, "ppm"}, {"tolueno", g_.tolueno, "ppm"},
                          {"acetona", g_.acetona, "ppm"}};
    int n = 0;
    for (const Dato& d : todos) if (n < max && ppmOk(d.valor)) out[n++] = d;
    return n;
  }

  void aApi(JsonDocument& doc) const override {
    JsonObject mq = doc.createNestedObject("mq135");
    if (fase_ != LISTO || g_.ts == 0) { mq["warmed_up"] = false; return; }
    mq["warmed_up"]      = true;
    mq["raw"]            = (int)acotar(g_.adcRaw, 0, 4095, 0);
    mq["output_voltage"] = acotar(g_.vAout, 0, 5.5, 3);
    if (isfinite(g_.rs) && g_.rs >= 0 && g_.rs <= 99999.999f) mq["rs_kohm"] = acotar(g_.rs, 0, 99999.999, 3);
    else mq["rs_kohm"] = nullptr;
    mq["alarm"] = nullptr;   // sin salida digital D-out

    const struct { const char* gas; float ppm; } lista[] = {
      {"CO2", g_.co2}, {"CO", g_.co}, {"NH3", g_.nh3},
      {"ALCOHOL", g_.alcohol}, {"TOLUENE", g_.tolueno}, {"ACETONE", g_.acetona}};
    JsonArray est = mq.createNestedArray("estimates");
    for (const auto& l : lista) {
      JsonObject e = est.createNestedObject();
      e["gas"] = l.gas;
      if (ppmOk(l.ppm)) e["ppm"] = acotar(l.ppm, 0, 99999999.99, 2);
      else              e["ppm"] = nullptr;
    }
  }
};
