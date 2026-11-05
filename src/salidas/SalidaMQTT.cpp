#include "SalidaMQTT.h"
#if BIORED_SALIDA_MQTT
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "../nucleo/Config.h"
#include "../nucleo/Registro.h"

namespace {
  WiFiClient   red;
  PubSubClient mqtt(red);
  EstadoMqtt   est = {false, 0, ""};
  TaskHandle_t tarea = nullptr;
  volatile bool reconectar = false;
  uint32_t ultIntento = 0, ultPub = 0;

  void topic(char* dst, size_t n, const char* sufijo) {
    snprintf(dst, n, "%s/%s/%s", cfg.mqttRaiz, cfg.mqttModulo, sufijo);
  }

  const char* describir(int rc) {
    switch (rc) {
      case -4: return "El broker no respondio a tiempo";
      case -3: return "Se perdio la conexion";
      case -2: return "No se pudo abrir la conexion (host, puerto o firewall)";
      case 1:  return "Protocolo rechazado";
      case 2:  return "Client ID rechazado";
      case 3:  return "Broker no disponible";
      case 4:  return "Usuario o clave incorrectos";
      case 5:  return "No autorizado";
      default: return "Error de conexion";
    }
  }

  void conectar() {
    mqtt.setServer(cfg.mqttHost, cfg.mqttPort);
    char estadoTopic[80]; topic(estadoTopic, sizeof estadoTopic, "estado");
    String m = WiFi.macAddress(); m.replace(":", "");
    const String cid = "esp32-" + m.substring(6);
    const bool conUsuario = cfg.mqttUser[0] != '\0';
    const bool ok = mqtt.connect(cid.c_str(),
                                 conUsuario ? cfg.mqttUser : nullptr,
                                 conUsuario ? cfg.mqttPass : nullptr,
                                 estadoTopic, 0, true, "offline");
    if (ok) {
      est.error[0] = '\0';
      mqtt.publish(estadoTopic, "online", true);
      Serial.printf("[MQTT] Conectado a %s:%u\n", cfg.mqttHost, cfg.mqttPort);
    } else {
      strlcpy(est.error, describir(mqtt.state()), sizeof est.error);
    }
  }

  void publicar() {
    for (int i = 0; i < registroCantidad(); i++) {
      Dato d[8];
      const int n = registroDatos(i, d, 8);
      if (n == 0) continue;
      StaticJsonDocument<384> doc;
      for (int k = 0; k < n; k++) doc[d[k].clave] = roundf(d[k].valor * 100.0f) / 100.0f;
      char payload[384], t[96];
      serializeJson(doc, payload, sizeof payload);
      topic(t, sizeof t, registroSensor(i)->id());
      if (mqtt.publish(t, payload)) est.publicados++;
    }
  }

  void tareaMqtt(void*) {
    for (;;) {
      if (reconectar) {
        reconectar = false;
        if (mqtt.connected()) mqtt.disconnect();
        ultIntento = millis() - 5000UL;     // reintenta de inmediato
        est.conectado = false;
      }
      const bool activa = cfg.mqttOn && cfg.mqttHost[0] && WiFi.status() == WL_CONNECTED;
      if (!activa) {
        if (mqtt.connected()) mqtt.disconnect();
        est.conectado = false;
        vTaskDelay(pdMS_TO_TICKS(1000));
        continue;
      }
      if (!mqtt.connected()) {
        est.conectado = false;
        if (millis() - ultIntento >= 5000UL) { ultIntento = millis(); conectar(); }
      } else {
        est.conectado = true;
        mqtt.loop();
        if (millis() - ultPub >= cfg.mqttIntervaloS * 1000UL) { ultPub = millis(); publicar(); }
      }
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}

void salidaMqttInit() {
  if (tarea) return;
  mqtt.setSocketTimeout(4);
  mqtt.setBufferSize(512);
  xTaskCreatePinnedToCore(tareaMqtt, "SalidaMQTT", 6144, nullptr, 1, &tarea, 0);
}

void salidaMqttAplicar() { reconectar = true; }
const EstadoMqtt& salidaMqttEstado() { return est; }
#endif
