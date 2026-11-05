#include "Red.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "../BioRedConfig.h"
#include "Config.h"

namespace {
  DNSServer dns;
  bool ap = false;
  uint32_t ultReintento = 0;

  void iniciarAP() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(BIORED_AP_SSID, BIORED_AP_PASS);
    dns.start(53, "*", WiFi.softAPIP());
    ap = true;
    Serial.printf("[Red] Portal: red '%s' -> http://%s\n", BIORED_AP_SSID, WiFi.softAPIP().toString().c_str());
  }

  void detenerAP() {
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    ap = false;
  }

  void anunciar() {
    Serial.printf("[Red] Conectado a %s, IP %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    if (MDNS.begin(BIORED_HOSTNAME)) MDNS.addService("http", "tcp", 80);
  }
}

void redInit() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);            // reduce latencia en módulos que requieren respuesta rápida
  if (cfg.wifiSsid[0]) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
    Serial.printf("[Red] Conectando a %s", cfg.wifiSsid);
    const uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) { delay(250); Serial.print("."); }
    Serial.println();
  }
  if (WiFi.status() == WL_CONNECTED) anunciar();
  else iniciarAP();
}

void redLoop() {
  if (!ap) return;
  dns.processNextRequest();
  if (WiFi.status() == WL_CONNECTED) { detenerAP(); anunciar(); return; }
  if (cfg.wifiSsid[0] && millis() - ultReintento > 60000) {   // reintenta la red guardada cada minuto
    ultReintento = millis();
    WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
  }
}

bool   redConectada() { return WiFi.status() == WL_CONNECTED; }
bool   redModoAP()    { return ap; }
String redIP()        { return redConectada() ? WiFi.localIP().toString() : ap ? WiFi.softAPIP().toString() : String("—"); }

String redRedesJson() {
  const int n = WiFi.scanComplete();
  if (n < 0) {                                   // sin escaneo previo o en curso
    if (n != WIFI_SCAN_RUNNING) WiFi.scanNetworks(true);
    return "{\"escaneando\":true,\"redes\":[]}";
  }
  StaticJsonDocument<1536> doc;
  doc["escaneando"] = false;
  JsonArray arr = doc.createNestedArray("redes");
  for (int i = 0; i < n && arr.size() < 15; i++) {
    const String s = WiFi.SSID(i);
    if (s.length() == 0) continue;
    bool repetida = false;
    for (JsonObject o : arr) if (s == o["ssid"].as<const char*>()) repetida = true;
    if (repetida) continue;
    JsonObject o = arr.createNestedObject();
    o["ssid"] = s;
    o["rssi"] = WiFi.RSSI(i);
  }
  WiFi.scanDelete();
  String out; serializeJson(doc, out);
  return out;
}
