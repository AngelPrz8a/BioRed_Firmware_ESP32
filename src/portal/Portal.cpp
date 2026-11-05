#include "Portal.h"
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "../BioRedConfig.h"
#include "../nucleo/Config.h"
#include "../nucleo/Red.h"
#include "../nucleo/Registro.h"
#include "../salidas/SalidaAPI.h"
#include "../salidas/SalidaMQTT.h"
#include "PortalHtml.h"
#if BIORED_MODULO_CAMARA
#include "../modulos/Camara.h"
#endif
#if BIORED_MODULO_MOTORES
#include "../modulos/Motores.h"
#endif

WebServer server(80);   // Camara.cpp lo usa con extern

namespace {
  // Con clave de portal configurada se pide usuario "admin". En modo AP no se pide
  // (la red de configuración ya está protegida y así nunca queda uno fuera).
  bool autorizado() {
    if (!cfg.portalPass[0] || redModoAP()) return true;
    if (server.authenticate("admin", cfg.portalPass)) return true;
    server.requestAuthentication();
    return false;
  }

  void enviarJson(const JsonDocument& doc, int codigo = 200) {
    String s; serializeJson(doc, s);
    server.send(codigo, "application/json", s);
  }

  void texto(JsonObject o, const char* k, char* dst, size_t n) {
    if (o.containsKey(k)) strlcpy(dst, o[k] | "", n);
  }
  void secreto(JsonObject o, const char* k, char* dst, size_t n) {   // vacío = sin cambios
    const char* s = o[k] | "";
    if (s[0]) strlcpy(dst, s, n);
  }
  void segmento(char* s) {                  // segmento de topic: a-z 0-9 - _
    for (char* p = s; *p; p++) {
      *p = tolower(*p);
      if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_')) *p = '-';
    }
  }
  uint32_t limitar(uint32_t v, uint32_t mn, uint32_t mx) { return v < mn ? mn : v > mx ? mx : v; }

  void handleRaiz() {
    if (autorizado()) server.send_P(200, "text/html", PORTAL_HTML);
  }

  void handleEstado() {
    if (!autorizado()) return;
    DynamicJsonDocument doc(3072);
    doc["version"] = BIORED_VERSION;
    doc["uptime"]  = millis() / 1000;
    doc["heap"]    = ESP.getFreeHeap();
    doc["camara"]  = BIORED_MODULO_CAMARA;
    doc["motores"] = BIORED_MODULO_MOTORES;

    JsonObject w = doc.createNestedObject("wifi");
    w["ok"] = redConectada(); w["ap"] = redModoAP(); w["ip"] = redIP(); w["rssi"] = WiFi.RSSI();

    const EstadoApi& a = salidaApiEstado();
    JsonObject ja = doc.createNestedObject("api");
    ja["on"] = BIORED_SALIDA_API && cfg.apiOn; ja["ultimo"] = a.ultimoCodigo; ja["enviados"] = a.enviados;

    const EstadoMqtt& m = salidaMqttEstado();
    JsonObject jm = doc.createNestedObject("mqtt");
    jm["on"] = BIORED_SALIDA_MQTT && cfg.mqttOn; jm["ok"] = m.conectado;
    jm["publicados"] = m.publicados; jm["error"] = m.error;

    JsonArray arr = doc.createNestedArray("sensores");
    for (int i = 0; i < registroCantidad(); i++) {
      JsonObject o = arr.createNestedObject();
      o["id"] = registroSensor(i)->id();
      o["nombre"] = registroSensor(i)->nombre();
      o["on"] = registroActivo(i);
      o["estado"] = registroSensor(i)->estado();
      JsonObject d = o.createNestedObject("datos");
      Dato buf[8];
      const int n = registroDatos(i, buf, 8);
      for (int k = 0; k < n; k++) d[buf[k].clave] = String(buf[k].valor, 1) + " " + buf[k].unidad;
    }
    enviarJson(doc);
  }

  void handleGetConfig() {   // nunca devuelve claves: solo si están guardadas
    if (!autorizado()) return;
    DynamicJsonDocument doc(2048);
    doc["wifi"]["ssid"] = cfg.wifiSsid;
    JsonObject a = doc.createNestedObject("api");
    a["on"] = cfg.apiOn; a["url"] = cfg.apiUrl; a["keySet"] = cfg.apiKey[0] != '\0'; a["intervalo"] = cfg.apiIntervaloS;
    JsonObject m = doc.createNestedObject("mqtt");
    m["on"] = cfg.mqttOn; m["host"] = cfg.mqttHost; m["port"] = cfg.mqttPort; m["user"] = cfg.mqttUser;
    m["passSet"] = cfg.mqttPass[0] != '\0'; m["raiz"] = cfg.mqttRaiz; m["modulo"] = cfg.mqttModulo; m["intervalo"] = cfg.mqttIntervaloS;
    doc["portalPassSet"] = cfg.portalPass[0] != '\0';
    JsonArray arr = doc.createNestedArray("sensores");
    for (int i = 0; i < registroCantidad(); i++) {
      JsonObject o = arr.createNestedObject();
      o["id"] = registroSensor(i)->id(); o["nombre"] = registroSensor(i)->nombre();
      o["on"] = registroActivo(i);       o["pin"] = registroPin(i);
    }
    enviarJson(doc);
  }

  void handlePostConfig() {
    if (!autorizado()) return;
    DynamicJsonDocument d(2048);
    if (deserializeJson(d, server.arg("plain"))) { server.send(400, "application/json", "{\"ok\":false}"); return; }
    bool reiniciar = d["reiniciar"] | false;

    JsonObject w = d["wifi"];
    if (!w.isNull()) {
      const char* ssid = w["ssid"] | "";
      if (ssid[0]) {
        const bool cambio = strcmp(ssid, cfg.wifiSsid) != 0;
        const char* pass = w["pass"] | "";
        strlcpy(cfg.wifiSsid, ssid, sizeof cfg.wifiSsid);
        if (cambio || pass[0]) strlcpy(cfg.wifiPass, pass, sizeof cfg.wifiPass);
        reiniciar = true;
      }
    }
    JsonObject a = d["api"];
    if (!a.isNull()) {
      cfg.apiOn = a["on"] | cfg.apiOn;
      texto(a, "url", cfg.apiUrl, sizeof cfg.apiUrl);
      if (!cfg.apiUrl[0]) strlcpy(cfg.apiUrl, BIORED_API_URL_DEFECTO, sizeof cfg.apiUrl);
      secreto(a, "key", cfg.apiKey, sizeof cfg.apiKey);
      cfg.apiIntervaloS = limitar(a["intervalo"] | cfg.apiIntervaloS, 10, 3600);
    }
    JsonObject m = d["mqtt"];
    if (!m.isNull()) {
      cfg.mqttOn = m["on"] | cfg.mqttOn;
      texto(m, "host", cfg.mqttHost, sizeof cfg.mqttHost);
      texto(m, "user", cfg.mqttUser, sizeof cfg.mqttUser);
      secreto(m, "pass", cfg.mqttPass, sizeof cfg.mqttPass);
      texto(m, "raiz", cfg.mqttRaiz, sizeof cfg.mqttRaiz);
      texto(m, "modulo", cfg.mqttModulo, sizeof cfg.mqttModulo);
      segmento(cfg.mqttRaiz); segmento(cfg.mqttModulo);
      if (!cfg.mqttRaiz[0]) strcpy(cfg.mqttRaiz, "biored");
      if (!cfg.mqttModulo[0]) strcpy(cfg.mqttModulo, "sensor-1");
      const uint32_t pt = m["port"] | (uint32_t)cfg.mqttPort;
      cfg.mqttPort = pt ? pt : 1883;
      cfg.mqttIntervaloS = limitar(m["intervalo"] | cfg.mqttIntervaloS, 2, 3600);
    }
    const char* pp = d["portalPass"] | "";
    if (pp[0]) strlcpy(cfg.portalPass, pp, sizeof cfg.portalPass);

    JsonArray ss = d["sensores"];
    if (!ss.isNull()) {
      for (JsonObject s : ss) registroConfigurar(s["id"] | "", s["on"] | false, s["pin"] | -1);
      reiniciar = true;   // los pines y sensores se aplican al arrancar
    }

    cfgGuardar();
    salidaMqttAplicar();
    server.send(200, "application/json", "{\"ok\":true}");
    if (reiniciar) { delay(500); ESP.restart(); }
  }

  void handleRedes() {
    if (autorizado()) server.send(200, "application/json", redRedesJson());
  }

#if BIORED_MODULO_MOTORES
  void handleMover() {
    if (!autorizado()) return;
    if (!server.hasArg("dir")) { server.send(400, "application/json", "{\"ok\":false,\"error\":\"Falta dir\"}"); return; }
    int pasos = server.hasArg("pasos") ? server.arg("pasos").toInt() : motoresPasosPorDefecto();
    String error;
    const bool ok = motoresMover(server.arg("dir"), pasos, error);
    StaticJsonDocument<128> r;
    r["ok"] = ok; r["pasos"] = pasos;
    if (!ok) r["error"] = error;
    enviarJson(r, ok ? 200 : 400);
  }
#endif
}

void portalInit() {
  server.on("/", HTTP_GET, handleRaiz);
  server.on("/api/estado", HTTP_GET, handleEstado);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/redes", HTTP_GET, handleRedes);
#if BIORED_MODULO_MOTORES
  server.on("/api/mover", HTTP_GET, handleMover);
#endif
#if BIORED_MODULO_CAMARA
  server.on("/resources/cam/cam-lo.jpg", HTTP_GET, [] { if (autorizado()) camaraHandleJpgLo(); });
  server.on("/resources/cam/cam-hi.jpg", HTTP_GET, [] { if (autorizado()) camaraHandleJpgHi(); });
  server.on("/resources/cam/live",       HTTP_GET, [] { if (autorizado()) camaraHandleStream(); });
#endif
  server.onNotFound([] {
    if (redModoAP()) {   // portal cautivo: cualquier ruta lleva al portal
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "application/json", "{\"error\":\"Ruta no encontrada\"}");
    }
  });
  server.begin();
  Serial.println("[Portal] Servidor web en el puerto 80");
}

void portalLoop() { server.handleClient(); }
