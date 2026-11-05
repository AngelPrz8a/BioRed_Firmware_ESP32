#pragma once
#include "Sensor.h"

namespace {
  const uint32_t MIC_PERIODO_US = 125;      // 8 kHz
  const uint32_t MIC_VENTANA_MS = 50;
  const uint32_t MIC_REPORTE_MS = 3000;
  const float    MIC_OFFSET_MIN = 1000.0f, MIC_OFFSET_MAX = 1500.0f, MIC_RMS_MIN = 0.1f;
  const float    MIC_GANANCIA_DB = 50.0f;
}

// El MAX9814 usa un pin ADC: la compatibilidad con WiFi depende del ADC y del
// pin disponible en la placa.
class SensorMicrofono : public Sensor {
  struct Ventana   { double suma = 0, sumaCuad = 0; uint32_t minimo = UINT32_MAX, maximo = 0, n = 0, inicioMs = 0; };
  struct Intervalo { double sumaRms2 = 0, sumaOffset = 0; float pico = 0; uint32_t ventanas = 0, muestras = 0, inicioMs = 0; };
  struct Lectura   { float rms = 0, pico = 0, db = 0, offset = 0; uint32_t fs = 0, ts = 0; bool valida = false; };

  int pin_ = 19;
  Ventana v_; Intervalo i_; Lectura l_;
  uint32_t proxUs_ = 0;

  void nuevaVentana()   { v_ = Ventana(); v_.inicioMs = millis(); }
  void nuevoIntervalo() { i_ = Intervalo(); i_.inicioMs = millis(); }

  void muestrear() {
    const uint32_t ahora = micros();
    if ((int32_t)(ahora - proxUs_) < 0) return;
    if (ahora - proxUs_ > MIC_PERIODO_US * 10) proxUs_ = ahora;   // el loop se atrasó
    proxUs_ += MIC_PERIODO_US;
    const uint32_t mv = analogReadMilliVolts(pin_);
    v_.suma += mv; v_.sumaCuad += (double)mv * mv;
    if (mv < v_.minimo) v_.minimo = mv;
    if (mv > v_.maximo) v_.maximo = mv;
    v_.n++;
  }

  void cerrarVentana() {
    if (millis() - v_.inicioMs < MIC_VENTANA_MS) return;
    if (v_.n) {
      const double media = v_.suma / v_.n;
      double var = v_.sumaCuad / v_.n - media * media;
      if (var < 0) var = 0;
      const float rms = sqrt(var), pico = (float)(v_.maximo - v_.minimo);
      i_.sumaRms2 += (double)rms * rms;
      i_.sumaOffset += media;
      if (pico > i_.pico) i_.pico = pico;
      i_.ventanas++; i_.muestras += v_.n;
    }
    nuevaVentana();
  }

  void consolidar() {
    if (millis() - i_.inicioMs < MIC_REPORTE_MS) return;
    if (i_.ventanas) {
      const uint32_t dur = millis() - i_.inicioMs;
      const float rms = sqrt(i_.sumaRms2 / i_.ventanas);
      l_.rms = rms;
      l_.pico = i_.pico;
      l_.db = 20.0 * log10(fmax(rms, MIC_RMS_MIN));
      l_.offset = i_.sumaOffset / i_.ventanas;
      l_.fs = (uint32_t)(i_.muestras * 1000.0 / dur);
      l_.valida = l_.offset >= MIC_OFFSET_MIN && l_.offset <= MIC_OFFSET_MAX;
      l_.ts = millis();
      if (!l_.valida) Serial.printf("[Audio] AVISO offset %.0f mV fuera de rango. Verificar el cableado en GPIO%d\n", l_.offset, pin_);
    }
    nuevoIntervalo();
  }

 public:
  const char* id() const override     { return "audio"; }
  const char* nombre() const override { return "MAX9814 (micrófono)"; }
  int pinPorDefecto() const override  { return 19; }

  void begin(int pin) override {
    pin_ = pin;
    nuevaVentana(); nuevoIntervalo();
    proxUs_ = micros();
  }

  void update() override { muestrear(); cerrarVentana(); consolidar(); }

  const char* estado() const override {
    return l_.ts == 0 ? "midiendo" : l_.valida ? "ok" : "verificar cableado";
  }

  int datos(Dato* out, int max) const override {
    if (l_.ts == 0 || max < 3) return 0;
    out[0] = {"nivel", l_.db,   "dB"};
    out[1] = {"rms",   l_.rms,  "mV"};
    out[2] = {"pico",  l_.pico, "mV"};
    return 3;
  }

  void aApi(JsonDocument& doc) const override {
    if (l_.ts == 0) return;
    JsonObject a = doc.createNestedObject("audio");
    a["valid"]           = l_.valida;
    a["rms_mv"]          = acotar(l_.rms, 0, 99999.99, 2);
    a["peak_to_peak_mv"] = acotar(l_.pico, 0, 99999.99, 2);
    a["offset_mv"]       = acotar(l_.offset, 0, 99999.9, 1);
    a["sample_rate_hz"]  = (uint32_t)(l_.fs > 65535 ? 65535 : l_.fs);
  }
};
