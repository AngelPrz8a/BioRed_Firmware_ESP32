#include "Config.h"
#include <Preferences.h>
#include "../BioRedConfig.h"

Cfg cfg;

namespace {
  const char* NS = "biored";
  void leer(Preferences& p, const char* k, char* dst, size_t n, const char* def) {
    strlcpy(dst, p.getString(k, def).c_str(), n);
  }
}

void cfgCargar() {
  Preferences p;
  p.begin(NS, false);
  leer(p, "ssid", cfg.wifiSsid, sizeof cfg.wifiSsid, "");
  leer(p, "wpass", cfg.wifiPass, sizeof cfg.wifiPass, "");
  cfg.apiOn = p.getBool("api_on", false);
  leer(p, "api_url", cfg.apiUrl, sizeof cfg.apiUrl, BIORED_API_URL_DEFECTO);
  leer(p, "api_key", cfg.apiKey, sizeof cfg.apiKey, "");
  leer(p, "uid", cfg.deviceUid, sizeof cfg.deviceUid, "");
  cfg.apiIntervaloS = p.getUInt("api_int", 30);
  cfg.mqttOn = p.getBool("mq_on", false);
  leer(p, "mq_host", cfg.mqttHost, sizeof cfg.mqttHost, "");
  leer(p, "mq_user", cfg.mqttUser, sizeof cfg.mqttUser, "");
  leer(p, "mq_pass", cfg.mqttPass, sizeof cfg.mqttPass, "");
  leer(p, "mq_raiz", cfg.mqttRaiz, sizeof cfg.mqttRaiz, "sensores");
  leer(p, "mq_mod", cfg.mqttModulo, sizeof cfg.mqttModulo, "sensor-1");
  cfg.mqttPort = p.getUShort("mq_port", 1883);
  cfg.mqttIntervaloS = p.getUInt("mq_int", 10);
  leer(p, "portal", cfg.portalPass, sizeof cfg.portalPass, "");
  p.end();
}

void cfgGuardar() {
  Preferences p;
  p.begin(NS, false);
  p.putString("ssid", cfg.wifiSsid);   p.putString("wpass", cfg.wifiPass);
  p.putBool("api_on", cfg.apiOn);      p.putString("api_url", cfg.apiUrl);
  p.putString("api_key", cfg.apiKey);  p.putString("uid", cfg.deviceUid);
  p.putUInt("api_int", cfg.apiIntervaloS);
  p.putBool("mq_on", cfg.mqttOn);      p.putString("mq_host", cfg.mqttHost);
  p.putString("mq_user", cfg.mqttUser); p.putString("mq_pass", cfg.mqttPass);
  p.putString("mq_raiz", cfg.mqttRaiz); p.putString("mq_mod", cfg.mqttModulo);
  p.putUShort("mq_port", cfg.mqttPort); p.putUInt("mq_int", cfg.mqttIntervaloS);
  p.putString("portal", cfg.portalPass);
  p.end();
}

bool cfgSensorOn(const char* id, bool def) {
  Preferences p; p.begin(NS, false);
  const bool v = p.getBool((String(id) + "_on").c_str(), def);
  p.end(); return v;
}
int cfgSensorPin(const char* id, int def) {
  Preferences p; p.begin(NS, false);
  const int v = p.getInt((String(id) + "_pin").c_str(), def);
  p.end(); return v;
}
void cfgSensorGuardar(const char* id, bool on, int pin) {
  Preferences p; p.begin(NS, false);
  p.putBool((String(id) + "_on").c_str(), on);
  p.putInt((String(id) + "_pin").c_str(), pin);
  p.end();
}
