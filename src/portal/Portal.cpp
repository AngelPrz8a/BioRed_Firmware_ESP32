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
    if (!configuracion.contrasenaPortal[0] || modoPuntoAcceso()) return true;
    if (server.authenticate("admin", configuracion.contrasenaPortal)) return true;
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
    w["ok"] = wifiConectado(); w["ap"] = modoPuntoAcceso(); w["ip"] = obtenerDireccionIp(); w["rssi"] = WiFi.RSSI();

    const EstadoEnvioApi& a = obtenerEstadoEnvioApi();
    JsonObject ja = doc.createNestedObject("api");
    ja["on"] = BIORED_SALIDA_API && configuracion.apiHabilitada; ja["ultimo"] = a.ultimoCodigo; ja["enviados"] = a.enviados;

    const EstadoConexionMqtt& m = obtenerEstadoMqtt();
    JsonObject jm = doc.createNestedObject("mqtt");
    jm["on"] = BIORED_SALIDA_MQTT && configuracion.mqttHabilitado; jm["ok"] = m.conectado;
    jm["publicados"] = m.publicados; jm["error"] = m.error;

    JsonArray arr = doc.createNestedArray("sensores");
    for (int i = 0; i < cantidadSensoresRegistrados(); i++) {
      JsonObject o = arr.createNestedObject();
      o["id"] = obtenerSensorRegistrado(i)->identificador();
      o["nombre"] = obtenerSensorRegistrado(i)->nombreParaMostrar();
      o["on"] = sensorRegistradoActivo(i);
      o["estado"] = obtenerSensorRegistrado(i)->estadoActual();
      JsonObject d = o.createNestedObject("datos");
      LecturaSensor buf[8];
      const int n = obtenerLecturasSensor(i, buf, 8);
      for (int k = 0; k < n; k++) d[buf[k].clave] = String(buf[k].valor, 1) + " " + buf[k].unidad;
    }
    enviarJson(doc);
  }

  void handleGetConfig() {   // nunca devuelve claves: solo si están guardadas
    if (!autorizado()) return;
    DynamicJsonDocument doc(2048);
    doc["wifi"]["ssid"] = configuracion.nombreRedWifi;
    JsonObject a = doc.createNestedObject("api");
    a["on"] = configuracion.apiHabilitada; a["url"] = configuracion.urlApi; a["keySet"] = configuracion.claveApi[0] != '\0'; a["intervalo"] = configuracion.intervaloApiSegundos;
    JsonObject m = doc.createNestedObject("mqtt");
    m["on"] = configuracion.mqttHabilitado; m["host"] = configuracion.servidorMqtt; m["port"] = configuracion.puertoMqtt; m["user"] = configuracion.usuarioMqtt;
    m["passSet"] = configuracion.contrasenaMqtt[0] != '\0'; m["raiz"] = configuracion.raizTopicsMqtt; m["modulo"] = configuracion.nombreEstacionMqtt; m["intervalo"] = configuracion.intervaloMqttSegundos;
    doc["portalPassSet"] = configuracion.contrasenaPortal[0] != '\0';
    JsonArray arr = doc.createNestedArray("sensores");
    for (int i = 0; i < cantidadSensoresRegistrados(); i++) {
      JsonObject o = arr.createNestedObject();
      o["id"] = obtenerSensorRegistrado(i)->identificador(); o["nombre"] = obtenerSensorRegistrado(i)->nombreParaMostrar();
      o["on"] = sensorRegistradoActivo(i);       o["pin"] = pinSensorRegistrado(i);
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
        const bool cambio = strcmp(ssid, configuracion.nombreRedWifi) != 0;
        const char* pass = w["pass"] | "";
        strlcpy(configuracion.nombreRedWifi, ssid, sizeof configuracion.nombreRedWifi);
        if (cambio || pass[0]) strlcpy(configuracion.contrasenaRedWifi, pass, sizeof configuracion.contrasenaRedWifi);
        reiniciar = true;
      }
    }
    JsonObject a = d["api"];
    if (!a.isNull()) {
      configuracion.apiHabilitada = a["on"] | configuracion.apiHabilitada;
      texto(a, "url", configuracion.urlApi, sizeof configuracion.urlApi);
      if (!configuracion.urlApi[0]) strlcpy(configuracion.urlApi, BIORED_API_URL_DEFECTO, sizeof configuracion.urlApi);
      secreto(a, "key", configuracion.claveApi, sizeof configuracion.claveApi);
      configuracion.intervaloApiSegundos = limitar(a["intervalo"] | configuracion.intervaloApiSegundos, 10, 3600);
    }
    JsonObject m = d["mqtt"];
    if (!m.isNull()) {
      configuracion.mqttHabilitado = m["on"] | configuracion.mqttHabilitado;
      texto(m, "host", configuracion.servidorMqtt, sizeof configuracion.servidorMqtt);
      texto(m, "user", configuracion.usuarioMqtt, sizeof configuracion.usuarioMqtt);
      secreto(m, "pass", configuracion.contrasenaMqtt, sizeof configuracion.contrasenaMqtt);
      texto(m, "raiz", configuracion.raizTopicsMqtt, sizeof configuracion.raizTopicsMqtt);
      texto(m, "modulo", configuracion.nombreEstacionMqtt, sizeof configuracion.nombreEstacionMqtt);
      segmento(configuracion.raizTopicsMqtt); segmento(configuracion.nombreEstacionMqtt);
      if (!configuracion.raizTopicsMqtt[0]) strcpy(configuracion.raizTopicsMqtt, "sensores");
      if (!configuracion.nombreEstacionMqtt[0]) strcpy(configuracion.nombreEstacionMqtt, "sensor-1");
      const uint32_t pt = m["port"] | (uint32_t)configuracion.puertoMqtt;
      configuracion.puertoMqtt = pt ? pt : 1883;
      configuracion.intervaloMqttSegundos = limitar(m["intervalo"] | configuracion.intervaloMqttSegundos, 2, 3600);
    }
    const char* pp = d["portalPass"] | "";
    if (pp[0]) strlcpy(configuracion.contrasenaPortal, pp, sizeof configuracion.contrasenaPortal);

    JsonArray ss = d["sensores"];
    if (!ss.isNull()) {
      for (JsonObject s : ss) configurarSensorRegistrado(s["id"] | "", s["on"] | false, s["pin"] | -1);
      reiniciar = true;   // los pines y sensores se aplican al arrancar
    }

    guardarConfiguracion();
    reconectarMqtt();
    server.send(200, "application/json", "{\"ok\":true}");
    if (reiniciar) { delay(500); ESP.restart(); }
  }

  void handleRedes() {
    if (autorizado()) server.send(200, "application/json", obtenerRedesComoJson());
  }

#if BIORED_MODULO_MOTORES
  void handleMover() {
    if (!autorizado()) return;
    if (!server.hasArg("dir")) { server.send(400, "application/json", "{\"ok\":false,\"error\":\"Falta dir\"}"); return; }
    int pasos = server.hasArg("pasos") ? server.arg("pasos").toInt() : pasosPredeterminadosMotor();
    String error;
    const bool ok = moverMotor(server.arg("dir"), pasos, error);
    StaticJsonDocument<128> r;
    r["ok"] = ok; r["pasos"] = pasos;
    if (!ok) r["error"] = error;
    enviarJson(r, ok ? 200 : 400);
  }
#endif
}

void iniciarPortal() {
  server.on("/", HTTP_GET, handleRaiz);
  server.on("/api/estado", HTTP_GET, handleEstado);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/redes", HTTP_GET, handleRedes);
#if BIORED_MODULO_MOTORES
  server.on("/api/mover", HTTP_GET, handleMover);
#endif
#if BIORED_MODULO_CAMARA
  server.on("/resources/cam/cam-lo.jpg", HTTP_GET, [] { if (autorizado()) enviarFotoCamaraBajaResolucion(); });
  server.on("/resources/cam/cam-hi.jpg", HTTP_GET, [] { if (autorizado()) enviarFotoCamaraAltaResolucion(); });
  server.on("/resources/cam/live",       HTTP_GET, [] { if (autorizado()) transmitirVideoCamara(); });
#endif
  server.onNotFound([] {
    if (modoPuntoAcceso()) {   // portal cautivo: cualquier ruta lleva al portal
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "application/json", "{\"error\":\"Ruta no encontrada\"}");
    }
  });
  server.begin();
  Serial.println("[Portal] Servidor web en el puerto 80");
}

void atenderPortal() { server.handleClient(); }
