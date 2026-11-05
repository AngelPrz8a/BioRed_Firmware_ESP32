#include "SalidaAPI.h"
#if BIORED_SALIDA_API
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../nucleo/Config.h"
#include "../nucleo/Registro.h"
#include "Certificados.h"

namespace {
  const uint32_t STACK_BYTES = 16384;      // TLS necesita pila amplia
  const uint32_t ESPERA_INICIAL_MS = 5000; // deja que cada sensor tenga su primera lectura
  const uint32_t TIMEOUT_MS = 10000;
  const uint32_t ESPERA_MAX_MS = 300000;   // tope del reintento con espera creciente
  const uint32_t ESPERA_ERROR_MS = 300000; // pausa tras errores de configuración

  EstadoApi    estado = {0, 0, 0};
  TaskHandle_t tarea = nullptr;
  char         uid[18] = "";
  uint8_t      fallos = 0;

  bool armarJson(String& out) {
    StaticJsonDocument<1536> doc;
    doc["device_uid"]       = uid;
    doc["firmware_version"] = BIORED_VERSION;
    doc["uptime_s"]         = (uint32_t)(millis() / 1000);
    if (!registroArmarApi(doc)) return false;
    if (doc.overflowed()) Serial.println("[API] AVISO el JSON no cupo en el documento");
    serializeJson(doc, out);
    return true;
  }

  int enviar(const String& cuerpo, String& respuesta) {
    WiFiClientSecure cliente;
    cliente.setCACert(CERTIFICADOS_RAIZ);
    cliente.setHandshakeTimeout(TIMEOUT_MS / 1000);
    HTTPClient http;
    if (!http.begin(cliente, cfg.apiUrl)) { respuesta = "URL invalida"; return -100; }
    http.setConnectTimeout(TIMEOUT_MS);
    http.setTimeout(TIMEOUT_MS);
    http.setUserAgent(String("BioRed-ESP32/") + BIORED_VERSION);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", cfg.apiKey);
    const int codigo = http.POST(cuerpo);
    if (codigo > 0) {
      respuesta = http.getString();
    } else {
      char tls[100] = "";
      cliente.lastError(tls, sizeof tls);
      respuesta = http.errorToString(codigo);
      if (tls[0]) respuesta += String(" | TLS ") + tls;
    }
    http.end();
    return codigo;
  }

  const char* pista(int c) {
    switch (c) {
      case 400: return "El JSON no cumple el contrato (ver error.details)";
      case 401: return "Key ausente o invalida (debe ser la de dispositivo)";
      case 403: return "Cloudflare bloqueo la peticion (regla WAF para /api/v1/readings/ingest)";
      case 404: return "Ruta no encontrada: revisa la URL";
      case 422: return "Estacion no registrada o sin sensor activo (revisa la MAC)";
      case 429: return "Demasiadas peticiones";
      default:  return c >= 500 ? "Falla del servidor" : "Sin conexion (wifi, DNS, certificado o tiempo de espera)";
    }
  }

  uint32_t siguienteEspera(int codigo, uint32_t base) {
    if (codigo == 201) { fallos = 0; return base; }
    if (codigo == 400 || codigo == 401 || codigo == 403 || codigo == 404 || codigo == 422) {
      fallos = 0; return ESPERA_ERROR_MS;     // reintentar no lo arregla
    }
    if (fallos < 5) fallos++;                 // 429, 5xx y sin conexión: espera creciente
    const uint32_t e = base << fallos;
    return e > ESPERA_MAX_MS ? ESPERA_MAX_MS : e;
  }

  void tareaEnvio(void*) {
    vTaskDelay(pdMS_TO_TICKS(ESPERA_INICIAL_MS));
    for (;;) {
      const uint32_t inicio = millis();
      const uint32_t base = cfg.apiIntervaloS * 1000UL;
      uint32_t espera = 2000;                 // inactiva: vuelve a mirar la config cada 2 s

      if (cfg.apiOn && cfg.apiKey[0] && WiFi.status() == WL_CONNECTED) {
        String cuerpo, respuesta;
        if (armarJson(cuerpo)) {
          const int codigo = enviar(cuerpo, respuesta);
          estado.ultimoCodigo = codigo;
          espera = siguienteEspera(codigo, base);
          if (codigo == 201) {
            estado.enviados++;
            Serial.printf("[API] 201 guardada en %lu ms (total %lu)\n", (unsigned long)(millis() - inicio), (unsigned long)estado.enviados);
          } else {
            estado.fallidos++;
            Serial.printf("[API] Error %d. %s\n[API] Respuesta: %.200s\n[API] Proximo intento en %lu s\n",
                          codigo, pista(codigo), respuesta.c_str(), (unsigned long)(espera / 1000));
          }
        } else {
          espera = 1000;
        }
      }
      const uint32_t transcurrido = millis() - inicio;
      vTaskDelay(pdMS_TO_TICKS(espera > transcurrido ? espera - transcurrido : 1000));
    }
  }
}

void salidaApiInit() {
  if (tarea) return;
  String mac = cfg.deviceUid[0] ? String(cfg.deviceUid) : WiFi.macAddress();   // MAC de fábrica = identidad
  mac.toUpperCase();
  strlcpy(uid, mac.c_str(), sizeof uid);
  if (xTaskCreatePinnedToCore(tareaEnvio, "SalidaAPI", STACK_BYTES, nullptr, 1, &tarea, 0) != pdPASS) {
    Serial.println("[API] No se pudo crear la tarea (memoria insuficiente)");
    tarea = nullptr;
  } else {
    Serial.printf("[API] Lista como %s\n", uid);
  }
}

const EstadoApi& salidaApiEstado() { return estado; }
#endif
