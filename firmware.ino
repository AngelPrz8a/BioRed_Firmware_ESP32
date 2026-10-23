/*
  ESP32 WROOM-32 - BioRed: Portal de configuración WiFi + Sensores + MQTT
  --------------------------------------------------------------------------
  Interfaz liviana con 4 pestañas:
    - Home            (/)               -> estado general: internet + sensores activos
    - Sensores        (/sensores)       -> activar/desactivar sensores y asignar pines
    - Red             (/red)            -> elegir/cambiar la red WiFi
    - Conexiones      (/conexiones)     -> configurar y conectar a un broker MQTT

  Librerías necesarias:
    - WiFi.h, WebServer.h, DNSServer.h, Preferences.h  (incluidas en el core ESP32)
    - DHT sensor library (Adafruit) + Adafruit Unified Sensor (Gestor de Librerías)
    - PubSubClient (Nick O'Leary) (Gestor de Librerías)

  Placa recomendada en Arduino IDE: "ESP32 Dev Module"
*/

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <DHT.h>
#include <PubSubClient.h>

// ---------- Configuración del Access Point ----------
const char* AP_SSID     = "ESP32-Setup";
const char* AP_PASSWORD = ""; // mínimo 8 caracteres, o "" para AP abierto
const byte  DNS_PORT    = 53;

WebServer server(80);
DNSServer  dnsServer;
Preferences prefs;

const unsigned long CONNECT_TIMEOUT_MS = 15000; // 15s para intentar conectar

bool internetActivo() {
  return WiFi.status() == WL_CONNECTED;
}

// Redes detectadas en el último escaneo
String redesSSID[30];
int redesRSSI[30];
int cantidadRedes = 0;

void escanearRedesUnaVez() {
  Serial.println("[WIFI] Escaneando redes...");
  unsigned long t0 = millis();
  int n = WiFi.scanNetworks();
  cantidadRedes = min(n, 30);
  for (int i = 0; i < cantidadRedes; i++) {
    redesSSID[i] = WiFi.SSID(i);
    redesRSSI[i] = WiFi.RSSI(i);
  }
  Serial.println("[WIFI] Escaneo tardo " + String(millis() - t0) + " ms, encontro " + String(cantidadRedes) + " redes");
}

String calidadSenal(int rssi) {
  if (rssi >= -50) return "Excelente";
  if (rssi >= -60) return "Muy buena";
  if (rssi >= -70) return "Buena";
  if (rssi >= -80) return "Débil";
  return "Muy débil";
}

// =====================================================================
// FRAMEWORK GENÉRICO DE SERVICIOS Y PINES (sensores/actuadores)
// =====================================================================

#define MAX_SERVICIOS        4
#define MAX_PINES_SERVICIO   3
#define MAX_OPCIONES         4

struct Servicio {
  String id;
  String nombre;
  String tipo;
  bool   habilitado;
  int    cantidadPines;
  String nombresPines[MAX_PINES_SERVICIO];
  int    pines[MAX_PINES_SERVICIO];       // -1 = sin asignar
  String canal;
  String tags;
  String payload;
  bool   tieneOpcion;
  String nombreOpcion;
  String opciones[MAX_OPCIONES];
  int    cantidadOpciones;
  String opcionSeleccionada;
};

Servicio servicios[MAX_SERVICIOS];
int cantidadServicios = 0;
int indiceServicioDHT = -1;
int indiceServicioMQTT = -1;
String ultimoIdProbado = "";      // último servicio testeado desde /sensores
String ultimoResultadoPrueba = ""; // resultado a mostrar una sola vez

// Pines GPIO recomendados para uso general en ESP32 WROOM-32
const int PINES_DISPONIBLES[] = {4, 5, 12, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
const int CANT_PINES_DISPONIBLES = sizeof(PINES_DISPONIBLES) / sizeof(PINES_DISPONIBLES[0]);

DHT* sensorDHT = nullptr;

// Estado cacheado de sensores para Home (no se lee el sensor en cada request)
bool dhtUltimoEstadoOK = false;
float dhtUltimaTemp = NAN;
float dhtUltimaHum = NAN;
bool dhtLecturaNueva = false;   // true = hay una lectura sin publicar por MQTT
unsigned long ultimaLecturaEstadoDHT = 0;
const unsigned long INTERVALO_LECTURA_ESTADO = 10000; // 10s

void inicializarServiciosDisponibles() {
  cantidadServicios = 0;

  Servicio& dht = servicios[cantidadServicios];
  dht.id = "dht";
  dht.nombre = "DHT (temperatura/humedad)";
  dht.tipo = "sensor";
  dht.habilitado = false;
  dht.cantidadPines = 1;
  dht.nombresPines[0] = "Pin de datos";
  dht.pines[0] = -1;
  dht.canal = "";
  dht.tags = "";
  dht.payload = "{valor}";
  dht.tieneOpcion = true;
  dht.nombreOpcion = "Modelo";
  dht.opciones[0] = "DHT11";
  dht.opciones[1] = "DHT22";
  dht.cantidadOpciones = 2;
  dht.opcionSeleccionada = "DHT22";
  indiceServicioDHT = cantidadServicios;
  cantidadServicios++;

  Servicio& mqtt = servicios[cantidadServicios];
  mqtt.id = "mqtt";
  mqtt.nombre = "Servicio MQTT";
  mqtt.tipo = "mqtt";
  mqtt.habilitado = false;
  mqtt.cantidadPines = 0;
  mqtt.canal = "biored/telemetry";
  mqtt.tags = "telemetria";
  mqtt.payload = "{\"service\":\"mqtt\",\"value\":\"ok\"}";
  mqtt.tieneOpcion = false;
  indiceServicioMQTT = cantidadServicios;
  cantidadServicios++;

  // Para agregar un servicio nuevo, copiar el bloque de arriba,
  // asignar un id y tipo únicos y añadir su lógica en los manejadores.
}

void cargarConfigServicios() {
  prefs.begin("config", true);
  for (int i = 0; i < cantidadServicios; i++) {
    servicios[i].habilitado = prefs.getBool((servicios[i].id + "_on").c_str(), false);
    for (int p = 0; p < servicios[i].cantidadPines; p++) {
      String key = servicios[i].id + "_p" + String(p);
      servicios[i].pines[p] = prefs.getInt(key.c_str(), -1);
    }
    servicios[i].canal = prefs.getString((servicios[i].id + "_canal").c_str(), servicios[i].canal);
    servicios[i].tags = prefs.getString((servicios[i].id + "_tags").c_str(), servicios[i].tags);
    servicios[i].payload = prefs.getString((servicios[i].id + "_payload").c_str(), servicios[i].payload);
    if (servicios[i].tieneOpcion) {
      String key = servicios[i].id + "_opt";
      servicios[i].opcionSeleccionada = prefs.getString(key.c_str(), servicios[i].opciones[0]);
    }
  }
  prefs.end();
}

void guardarConfigServicio(int idx) {
  prefs.begin("config", false);
  prefs.putBool((servicios[idx].id + "_on").c_str(), servicios[idx].habilitado);
  for (int p = 0; p < servicios[idx].cantidadPines; p++) {
    String key = servicios[idx].id + "_p" + String(p);
    prefs.putInt(key.c_str(), servicios[idx].pines[p]);
  }
  prefs.putString((servicios[idx].id + "_canal").c_str(), servicios[idx].canal);
  prefs.putString((servicios[idx].id + "_tags").c_str(), servicios[idx].tags);
  prefs.putString((servicios[idx].id + "_payload").c_str(), servicios[idx].payload);
  if (servicios[idx].tieneOpcion) {
    String key = servicios[idx].id + "_opt";
    prefs.putString(key.c_str(), servicios[idx].opcionSeleccionada);
  }
  prefs.end();
}

void reinicializarSensorDHT() {
  if (sensorDHT != nullptr) {
    delete sensorDHT;
    sensorDHT = nullptr;
  }

  Servicio& s = servicios[indiceServicioDHT];
  if (!s.habilitado || s.pines[0] == -1) return;

  uint8_t tipo = (s.opcionSeleccionada == "DHT11") ? DHT11 : DHT22;
  sensorDHT = new DHT(s.pines[0], tipo);
  sensorDHT->begin();
  Serial.println("[DHT] Sensor inicializado en GPIO " + String(s.pines[0]) + " (" + s.opcionSeleccionada + ")");
}

// Actualiza el estado cacheado de cada sensor (se llama periódicamente desde loop(),
// nunca desde el manejador de /  para no bloquear la respuesta HTTP con una lectura)
void actualizarEstadoSensores() {
  if (sensorDHT == nullptr) return;
  float t = sensorDHT->readTemperature();
  float h = sensorDHT->readHumidity();
  dhtUltimoEstadoOK = !(isnan(t) || isnan(h));
  if (dhtUltimoEstadoOK) {
    dhtUltimaTemp = t;
    dhtUltimaHum = h;
    dhtLecturaNueva = true;
  }
}

// =====================================================================
// MQTT
// =====================================================================

String mqttHost = "";
int mqttPort = 1883;
bool mqttHabilitado = false;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long ultimoIntentoMQTT = 0;
unsigned long ultimaPublicacionMQTT = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;
const unsigned long MQTT_PUBLISH_INTERVAL_MS = 5000;

// --- Topic de prueba y diagnóstico de errores ---
const char* MQTT_TOPIC_PRUEBA_DEFAULT = "biored/lab/hello";
String mqttTopicPrueba = MQTT_TOPIC_PRUEBA_DEFAULT;   // editable desde /conexiones
String mqttUltimoError = "";          // vacío = sin error
int    mqttUltimoRc = 0;
unsigned long mqttUltimoErrorMs = 0;
int    mqttIntentosFallidos = 0;
String mqttUltimoMensaje = "";        // último mensaje recibido en el topic de prueba
String mqttResultadoPrueba = "";      // HTML del resultado de "Probar", se muestra una sola vez
bool   mqttResultadoPruebaOK = false;
bool   mqttEcoRecibido = false;       // usado por la prueba manual

// Seguimiento del servicio MQTT (canal/tags/payload) para mostrarlo en /sensores
unsigned long svcUltimaPubMs = 0;
bool          svcUltimaPubOK = false;
unsigned long svcPubTotal = 0;

// Seguimiento de las publicaciones del DHT
unsigned long dhtUltimaPubMs = 0;
bool          dhtUltimaPubOK = false;
unsigned long dhtPubTotal = 0;

// --- Estructura de topics: <raiz>/<modulo>/<dato> ---
//   biored/lab/hello               (pruebas)
//   biored/sensor-1/temperatura    (sensores)
//   biored/sensor-1/humedad
const char* MQTT_RAIZ_DEFAULT = "biored";
const char* MQTT_MODULO_DEFAULT = "sensor-1";
String mqttRaiz = MQTT_RAIZ_DEFAULT;
String mqttModulo = MQTT_MODULO_DEFAULT;

// Un segmento de topic solo admite minúsculas, números, - y _ (sin "/", "+", "#" ni espacios)
String sanitizarSegmento(String s, const char* porDefecto) {
  s.trim();
  s.toLowerCase();
  String o = "";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_') o += c;
    else if (c == ' ') o += '-';
  }
  if (o == "") o = porDefecto;
  return o;
}

String topicModulo(const String& dato) {
  return mqttRaiz + "/" + mqttModulo + "/" + dato;
}

String topicPruebaPorDefecto() {
  return mqttRaiz + "/lab/hello";
}

void cargarConfigMQTT() {
  prefs.begin("mqtt", true);
  mqttHost = prefs.getString("host", "");
  mqttPort = prefs.getInt("port", 1883);
  mqttHabilitado = prefs.getBool("on", false);
  mqttRaiz = prefs.getString("root", MQTT_RAIZ_DEFAULT);
  mqttModulo = prefs.getString("mod", MQTT_MODULO_DEFAULT);
  mqttTopicPrueba = prefs.getString("topic", "");
  if (mqttTopicPrueba == "") mqttTopicPrueba = mqttRaiz + "/lab/hello";
  prefs.end();
}

void guardarConfigMQTT() {
  prefs.begin("mqtt", false);
  prefs.putString("host", mqttHost);
  prefs.putInt("port", mqttPort);
  prefs.putBool("on", mqttHabilitado);
  prefs.putString("topic", mqttTopicPrueba);
  prefs.putString("root", mqttRaiz);
  prefs.putString("mod", mqttModulo);
  prefs.end();
}

String descripcionErrorMQTT(int rc) {
  switch (rc) {
    case -4: return "Timeout: el broker no respondió a tiempo";
    case -3: return "Se perdió la conexión con el broker";
    case -2: return "No se pudo abrir la conexión con el broker (host o puerto incorrecto, broker apagado o bloqueado por firewall)";
    case -1: return "Desconectado del broker";
    case 1:  return "El broker rechazó la versión del protocolo MQTT";
    case 2:  return "El broker rechazó el client ID";
    case 3:  return "El broker no está disponible";
    case 4:  return "Usuario o contraseña incorrectos";
    case 5:  return "No autorizado por el broker";
    default: return "Error desconocido";
  }
}

void registrarErrorMQTT(const String& msg, int rc) {
  if (msg != mqttUltimoError) Serial.println("[MQTT] ERROR: " + msg);
  mqttUltimoError = msg;
  mqttUltimoRc = rc;
  mqttUltimoErrorMs = millis();
  mqttIntentosFallidos++;
}

void limpiarErrorMQTT() {
  mqttUltimoError = "";
  mqttUltimoRc = 0;
  mqttIntentosFallidos = 0;
}

// Verifica lo que hace falta ANTES de abrir el socket: host cargado,
// WiFi conectado y nombre de host resoluble por DNS.
bool verificarPrerequisitosMQTT(const String& host, String& motivo) {
  if (host == "") {
    motivo = "Falta el host / IP del broker";
    return false;
  }
  if (!internetActivo()) {
    motivo = "Sin WiFi: el ESP32 no está conectado a ninguna red";
    return false;
  }
  IPAddress ip;
  if (!WiFi.hostByName(host.c_str(), ip)) {
    motivo = "No se pudo resolver el host \"" + host + "\" (DNS). Revisá el nombre o usá la IP directa";
    return false;
  }
  return true;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length && i < 120; i++) msg += (char)payload[i];
  mqttUltimoMensaje = String(topic) + ": " + msg;
  Serial.println("[MQTT] Mensaje recibido en " + mqttUltimoMensaje);
}

void intentarConectarMQTT() {
  if (!mqttHabilitado) return;
  if (mqttClient.connected()) return;

  String motivo;
  if (!verificarPrerequisitosMQTT(mqttHost, motivo)) {
    registrarErrorMQTT(motivo, 0);
    return;
  }

  String clientId = "ESP32-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
  Serial.println("[MQTT] Conectando a " + mqttHost + ":" + String(mqttPort) + " como " + clientId);

  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("[MQTT] Conectado");
    limpiarErrorMQTT();

    // Al conectar: suscribirse al topic de prueba y publicar un "hello"
    if (!mqttClient.subscribe(mqttTopicPrueba.c_str())) {
      registrarErrorMQTT("Conectó, pero falló la suscripción a \"" + mqttTopicPrueba + "\"", 0);
    }
    String hello = "{\"module\":\"" + mqttModulo + "\",\"device\":\"" + clientId + "\",\"msg\":\"hello\"}";
    if (!mqttClient.publish(mqttTopicPrueba.c_str(), hello.c_str())) {
      registrarErrorMQTT("Conectó, pero falló la publicación en \"" + mqttTopicPrueba + "\" (topic demasiado largo o conexión caída)", 0);
    }
  } else {
    int rc = mqttClient.state();
    registrarErrorMQTT(descripcionErrorMQTT(rc), rc);
  }
}

void aplicarConfigMQTT() {
  mqttClient.setServer(mqttHost.c_str(), mqttPort);
  mqttClient.setSocketTimeout(5);   // por defecto son 15 s y bloquea todo el loop
  mqttClient.setBufferSize(1024);   // por defecto 256 bytes: un payload editable puede superarlo
  mqttClient.setCallback(mqttCallback);
  if (mqttClient.connected()) mqttClient.disconnect();
  limpiarErrorMQTT();
  mqttUltimoMensaje = "";
  intentarConectarMQTT();
}

String construirPayloadMQTT(const struct Servicio& servicio) {
  if (servicio.tags == "") return servicio.payload;

  String tagsJson = "[";
  int posicion = 0;
  String copia = servicio.tags;
  while (true) {
    int coma = copia.indexOf(',');
    String tag = coma >= 0 ? copia.substring(0, coma) : copia;
    tag.trim();
    if (tag != "") {
      if (posicion++ > 0) tagsJson += ',';
      tagsJson += '"' + tag + '"';
    }
    if (coma < 0) break;
    copia = copia.substring(coma + 1);
  }
  tagsJson += "]";

  return "{\"service\":\"" + servicio.id + "\",\"tags\":" + tagsJson
       + ",\"payload\":" + servicio.payload + "}";
}

// Canal base del DHT: el configurado o, si está vacío, <raiz>/<modulo>
String canalBaseDHTPorDefecto() {
  return mqttRaiz + "/" + mqttModulo;
}

String canalBaseDHT() {
  Servicio& s = servicios[indiceServicioDHT];
  return (s.canal != "") ? s.canal : canalBaseDHTPorDefecto();
}

// Arma el mensaje del DHT: reemplaza {valor}, {dato} y {unidad} en el payload y,
// si hay etiquetas, lo envuelve igual que el servicio MQTT.
String construirPayloadDHT(const struct Servicio& s, const String& dato, const String& unidad, float valor) {
  String p = (s.payload == "") ? String("{valor}") : s.payload;
  p.replace("{valor}", String(valor, 1));
  p.replace("{dato}", dato);
  p.replace("{unidad}", unidad);
  Servicio copia = s;
  copia.payload = p;
  return construirPayloadMQTT(copia);
}

// Publica la última lectura del DHT (cacheada, no bloquea) en
// <canal base>/temperatura y <canal base>/humedad
void publicarSensoresMQTT() {
  if (!mqttHabilitado || !mqttClient.connected()) return;
  if (sensorDHT == nullptr || !dhtLecturaNueva) return;
  dhtLecturaNueva = false;

  Servicio& s = servicios[indiceServicioDHT];
  String base = canalBaseDHT();
  String tTemp = base + "/temperatura";
  String tHum  = base + "/humedad";
  String pTemp = construirPayloadDHT(s, "temperatura", "°C", dhtUltimaTemp);
  String pHum  = construirPayloadDHT(s, "humedad", "%", dhtUltimaHum);

  bool okT = mqttClient.publish(tTemp.c_str(), pTemp.c_str());
  bool okH = mqttClient.publish(tHum.c_str(), pHum.c_str());

  dhtUltimaPubMs = millis();
  dhtUltimaPubOK = okT && okH;
  dhtPubTotal++;

  if (!okT) registrarErrorMQTT("Falló la publicación en \"" + tTemp + "\" (canal inválido, payload demasiado grande o conexión caída)", 0);
  if (!okH) registrarErrorMQTT("Falló la publicación en \"" + tHum + "\" (canal inválido, payload demasiado grande o conexión caída)", 0);
  if (okT && okH && mqttUltimoError.startsWith("Falló la publicación")) limpiarErrorMQTT();
}

void publicarServicioMQTT() {
  if (!mqttHabilitado || !mqttClient.connected()) return;
  if (millis() - ultimaPublicacionMQTT < MQTT_PUBLISH_INTERVAL_MS) return;
  ultimaPublicacionMQTT = millis();

  for (int i = 0; i < cantidadServicios; i++) {
    Servicio& s = servicios[i];
    if (s.tipo != "mqtt" || !s.habilitado || s.canal == "") continue;

    String payload = construirPayloadMQTT(s);
    bool ok = mqttClient.publish(s.canal.c_str(), payload.c_str());
    svcUltimaPubMs = millis();
    svcUltimaPubOK = ok;
    svcPubTotal++;
    if (!ok) {
      Serial.println("[MQTT] Error al publicar en " + s.canal);
      registrarErrorMQTT("Falló la publicación en \"" + s.canal + "\" (canal inválido, payload demasiado grande o conexión caída)", 0);
    } else if (mqttUltimoError.startsWith("Falló la publicación")) {
      limpiarErrorMQTT();
    }
  }
}

// =====================================================================
// UTILIDADES HTML (tema claro, navbar)
// =====================================================================

String htmlHeader(const String& titulo) {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>)rawliteral";
  html += titulo;
  html += R"rawliteral(</title>
<style>
  * { box-sizing: border-box; }
  body {
    font-family: -apple-system, Segoe UI, Roboto, Arial, sans-serif;
    background: #f8fafc;
    color: #0f172a;
    margin: 0;
  }
  .navbar {
    display:flex;
    justify-content:space-between;
    align-items:center;
    padding: 14px 20px;
    background:#ffffff;
    border-bottom: 1px solid #e2e8f0;
  }
  .navbar .brand { font-weight:700; font-size:16px; color:#0f172a; text-decoration:none; }
  .navbar nav a {
    margin-left: 16px;
    text-decoration:none;
    color:#64748b;
    font-size: 13px;
  }
  .navbar nav a.activo { color:#0f172a; font-weight:600; }
  .container {
    max-width: 420px;
    margin: 20px auto;
    background:#fff;
    border:1px solid #e2e8f0;
    border-radius: 12px;
    padding: 22px 20px;
  }
  h1 { font-size: 17px; margin: 0 0 4px; }
  p.sub { color: #64748b; font-size: 13px; margin: 0 0 18px; }
  label { display:block; font-size: 13px; margin: 12px 0 6px; color:#475569; }
  select, input[type=password], input[type=text], input[type=number], textarea {
    width: 100%;
    padding: 9px 10px;
    border-radius: 8px;
    border: 1px solid #cbd5e1;
    background: #fff;
    color: #0f172a;
    font-size: 14px;
  }
  button {
    margin-top: 18px;
    width: 100%;
    padding: 11px;
    border: none;
    border-radius: 8px;
    background: #0f172a;
    color: #fff;
    font-weight: 600;
    font-size: 14px;
    cursor: pointer;
  }
  button:active { transform: scale(0.98); }
  .status-icon { font-size: 40px; text-align:center; margin-bottom: 8px; }
  .ok    { color:#16a34a; }
  .fail  { color:#dc2626; }
  .row {
    display:flex;
    justify-content:space-between;
    align-items:center;
    padding: 9px 0;
    border-bottom: 1px solid #f1f5f9;
    font-size: 14px;
  }
  .row:last-child { border-bottom: none; }
  .row .label { color:#64748b; }
  .badge {
    display:inline-block;
    padding: 3px 10px;
    border-radius: 999px;
    font-size: 12px;
    font-weight: 600;
  }
  .badge-activa   { background:#dcfce7; color:#16a34a; }
  .badge-inactiva { background:#fee2e2; color:#dc2626; }
  a.link {
    display:block;
    text-align:center;
    margin-top: 14px;
    color:#2563eb;
    text-decoration:none;
    font-size: 13px;
  }
  .svc-box {
    border:1px solid #e2e8f0;
    border-radius: 10px;
    padding: 14px;
    margin-bottom: 14px;
  }
  .toggle-row {
    display:flex;
    justify-content:space-between;
    align-items:center;
  }
  .switch { position:relative; display:inline-block; width:38px; height:21px; flex-shrink:0; }
  .switch input { opacity:0; width:0; height:0; }
  .slider {
    position:absolute; cursor:pointer; inset:0;
    background:#cbd5e1; border-radius:999px; transition:.15s;
  }
  .slider:before {
    content:''; position:absolute; height:15px; width:15px;
    left:3px; top:3px; background:#fff; border-radius:50%; transition:.15s;
  }
  input:checked + .slider { background:#16a34a; }
  input:checked + .slider:before { transform: translateX(17px); }
  button.secundario { background:#e2e8f0; color:#0f172a; margin-top:10px; }
  .empty { color:#94a3b8; font-size:13px; }
  .alert {
    border-radius: 10px;
    padding: 12px 14px;
    margin: 14px 0 4px;
    font-size: 13px;
    line-height: 1.5;
    word-break: break-word;
  }
  .alert-error { background:#fef2f2; border:1px solid #fecaca; color:#991b1b; }
  .alert-ok    { background:#f0fdf4; border:1px solid #bbf7d0; color:#166534; }
  .alert small { color:inherit; opacity:.8; }
  .hint { color:#94a3b8; font-size:12px; margin:6px 0 0; }
  textarea { font-family: ui-monospace, Menlo, Consolas, monospace; font-size:13px; resize:vertical; }
  .code {
    background:#f8fafc; border:1px solid #e2e8f0; border-radius:8px;
    padding:8px 10px; margin:0; font-size:12px;
    font-family: ui-monospace, Menlo, Consolas, monospace;
    white-space:pre-wrap; word-break:break-all; color:#334155;
  }
</style>
</head>
<body>
)rawliteral";
  return html;
}

String htmlFooter() {
  return "</body></html>";
}

// Escapa texto que va dentro de HTML (hosts, topics y mensajes del broker
// son datos externos y no deben interpretarse como marcado).
String esc(const String& s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&':  o += "&amp;";  break;
      case '<':  o += "&lt;";   break;
      case '>':  o += "&gt;";   break;
      case '"':  o += "&quot;"; break;
      case '\'': o += "&#39;";  break;
      default:   o += c;
    }
  }
  return o;
}

// Recuadro rojo con el último error MQTT (vacío si no hay error)
String htmlAlertaErrorMQTT() {
  if (!mqttHabilitado || mqttUltimoError == "") return "";
  String h = "<div class='alert alert-error'><strong>Error de conexión MQTT</strong><br>" + esc(mqttUltimoError);
  h += "<br><small>Intentos fallidos: " + String(mqttIntentosFallidos);
  if (mqttUltimoRc != 0) h += " &middot; rc=" + String(mqttUltimoRc);
  h += " &middot; hace " + String((millis() - mqttUltimoErrorMs) / 1000) + " s</small></div>";
  return h;
}

// Estado de la conexión + broker + última publicación + errores (para cada servicio)
String htmlEstadoMQTT(unsigned long pubTotal, bool pubOK, unsigned long pubMs) {
  String h = "<div class='row'><span class='label'>Conexión MQTT</span>";
  if (!mqttHabilitado) {
    h += "<span class='badge badge-inactiva'>Deshabilitado</span>";
  } else {
    h += mqttClient.connected()
           ? "<span class='badge badge-activa'>Conectado</span>"
           : "<span class='badge badge-inactiva'>Desconectado</span>";
  }
  h += "</div>";

  h += "<div class='row'><span class='label'>Broker</span><span style='font-size:13px;'>";
  h += (mqttHost == "") ? String("sin configurar") : esc(mqttHost) + ":" + String(mqttPort);
  h += "</span></div>";

  h += "<div class='row'><span class='label'>Última publicación</span><span style='font-size:13px;'>";
  if (pubTotal == 0) {
    h += "&mdash;";
  } else {
    h += String(pubOK ? "&#10003; " : "&#10007; ") + "hace " + String((millis() - pubMs) / 1000) + " s (" + String(pubTotal) + " en total)";
  }
  h += "</span></div>";

  if (!mqttHabilitado) {
    h += "<p class='hint'>El MQTT está deshabilitado: activalo en <a href='/conexiones'>Conexiones</a> para que este servicio publique.</p>";
  }
  h += htmlAlertaErrorMQTT();
  return h;
}

String navBar(const String& activa) {
  String html = "<div class='navbar'><a href='/' class='brand'>BioRed</a><nav>";
  html += "<a href='/sensores' class='" + String(activa == "sensores" ? "activo" : "") + "'>Sensores</a>";
  html += "<a href='/red' class='" + String(activa == "red" ? "activo" : "") + "'>Red</a>";
  html += "<a href='/conexiones' class='" + String(activa == "conexiones" ? "activo" : "") + "'>Conexiones</a>";
  html += "</nav></div>";
  return html;
}

// =====================================================================
// PÁGINA: HOME (/)
// =====================================================================

void handleHome() {
  Serial.println("[HTTP] GET / de " + server.client().remoteIP().toString());

  String html = htmlHeader("BioRed");
  html += navBar("home");
  html += "<div class='container'>";
  html += "<h1>BioRed</h1>";
  html += "<p class='sub'>Estado general del sistema</p>";

  bool activa = internetActivo();
  html += "<div class='row'><span class='label'>Internet</span>";
  html += activa ? "<span class='badge badge-activa'>Activo</span>" : "<span class='badge badge-inactiva'>Inactivo</span>";
  html += "</div>";
  if (activa) {
    html += "<div class='row'><span class='label'>Red</span><span>" + WiFi.SSID() + "</span></div>";
  }

  html += "<div class='row'><span class='label'>MQTT</span>";
  if (!mqttHabilitado) {
    html += "<span class='badge badge-inactiva'>Deshabilitado</span>";
  } else {
    html += mqttClient.connected()
              ? "<span class='badge badge-activa'>Conectado</span>"
              : "<span class='badge badge-inactiva'>Desconectado</span>";
  }
  html += "</div>";

  html += "<p style='margin-top:16px;font-size:13px;color:#64748b;'>Sensores</p>";

  // Solo se muestra nombre + estado (activo/con fallas), sin datos concretos:
  // las lecturas se observan en otra página para no sobrecargar el ESP.
  bool hayAlguno = false;
  for (int i = 0; i < cantidadServicios; i++) {
    if (servicios[i].habilitado && servicios[i].pines[0] != -1) {
      hayAlguno = true;
      html += "<div class='row'><span class='label'>" + servicios[i].nombre + "</span>";

      if (servicios[i].id == "dht" && sensorDHT != nullptr) {
        html += dhtUltimoEstadoOK
                  ? "<span class='badge badge-activa'>Activo</span>"
                  : "<span class='badge badge-inactiva'>Con fallas</span>";
      } else {
        html += "<span class='badge badge-activa'>Activo</span>";
      }
      html += "</div>";
    }
  }
  if (!hayAlguno) {
    html += "<p class='empty'>Ningún sensor habilitado todavía</p>";
  }

  html += "<a class='link' href='/memoria'>&#128202; Ver memoria del ESP32</a>";

  html += "</div>";
  html += htmlFooter();
  server.send(200, "text/html", html);
}

// =====================================================================
// PÁGINA: SENSORES (/sensores)
// =====================================================================

void handleSensores() {
  String html = htmlHeader("BioRed - Sensores");
  html += navBar("sensores");
  html += "<div class='container'>";
  html += "<h1>Sensores</h1>";
  html += "<p class='sub'>Activá cada sensor y asigná su pin</p>";
  html += "<form action='/guardar-sensores' method='POST'>";

  for (int i = 0; i < cantidadServicios; i++) {
    Servicio& s = servicios[i];
    html += "<div class='svc-box'>";

    html += "<div class='toggle-row'><strong>" + s.nombre + "</strong>";
    html += "<label class='switch'><input type='checkbox' name='" + s.id + "_hab'";
    if (s.habilitado) html += " checked";
    html += "><span class='slider'></span></label></div>";

    for (int p = 0; p < s.cantidadPines; p++) {
      String nombreCampo = s.id + "_p" + String(p);
      html += "<label>" + s.nombresPines[p] + "</label>";
      html += "<select name='" + nombreCampo + "'>";
      html += "<option value='-1'" + String(s.pines[p] == -1 ? " selected" : "") + ">Seleccionar</option>";
      for (int k = 0; k < CANT_PINES_DISPONIBLES; k++) {
        int gpio = PINES_DISPONIBLES[k];
        html += "<option value='" + String(gpio) + "'";
        if (s.pines[p] == gpio) html += " selected";
        html += ">GPIO " + String(gpio) + "</option>";
      }
      html += "</select>";
    }

    if (s.tieneOpcion) {
      String nombreCampoOpt = s.id + "_opt";
      html += "<label>" + s.nombreOpcion + "</label>";
      html += "<select name='" + nombreCampoOpt + "'>";
      for (int o = 0; o < s.cantidadOpciones; o++) {
        html += "<option value='" + s.opciones[o] + "'";
        if (s.opcionSeleccionada == s.opciones[o]) html += " selected";
        html += ">" + s.opciones[o] + "</option>";
      }
      html += "</select>";
    }

    if (s.tipo == "mqtt") {
      html += htmlEstadoMQTT(svcPubTotal, svcUltimaPubOK, svcUltimaPubMs);

      // --- Campos editables ---
      html += "<label>Canal / topic MQTT</label>";
      html += "<input type='text' name='" + s.id + "_canal' value='" + esc(s.canal) + "' placeholder='" + esc(topicModulo("telemetria")) + "'>";
      html += "<p class='hint'>Estructura: raíz/módulo/dato (ej. " + esc(topicModulo("telemetria")) + ")</p>";
      html += "<label>Etiquetas (tags, separadas por coma)</label>";
      html += "<input type='text' name='" + s.id + "_tags' value='" + esc(s.tags) + "' placeholder='temperatura,humedad'>";
      html += "<label>Payload</label>";
      html += "<textarea name='" + s.id + "_payload' rows='3'>" + esc(s.payload) + "</textarea>";
      html += "<p class='hint'>Con etiquetas, el payload debe ser JSON válido (se envía junto con service y tags). Sin etiquetas se envía tal cual.</p>";

      html += "<label>Mensaje que se enviará (según lo guardado)</label>";
      html += "<pre class='code'>" + esc(construirPayloadMQTT(s)) + "</pre>";
    }

    if (s.id == "dht") {
      // --- Estado del sensor y de la conexión ---
      html += "<div class='row'><span class='label'>Sensor</span>";
      if (!s.habilitado) {
        html += "<span class='badge badge-inactiva'>Deshabilitado</span>";
      } else if (s.pines[0] == -1) {
        html += "<span class='badge badge-inactiva'>Sin pin</span>";
      } else if (sensorDHT == nullptr) {
        html += "<span class='badge badge-inactiva'>Sin inicializar</span>";
      } else {
        html += dhtUltimoEstadoOK
                  ? "<span class='badge badge-activa'>Activo</span>"
                  : "<span class='badge badge-inactiva'>Con fallas</span>";
      }
      html += "</div>";
      html += htmlEstadoMQTT(dhtPubTotal, dhtUltimaPubOK, dhtUltimaPubMs);

      // --- Campos editables ---
      String base = canalBaseDHT();
      html += "<label>Canal base / topic MQTT</label>";
      html += "<input type='text' name='dht_canal' value='" + esc(base) + "' placeholder='" + esc(canalBaseDHTPorDefecto()) + "'>";
      html += "<p class='hint'>Publica en <b>" + esc(base) + "/temperatura</b> y <b>" + esc(base) + "/humedad</b>.</p>";
      html += "<label>Etiquetas (tags, separadas por coma)</label>";
      html += "<input type='text' name='dht_tags' value='" + esc(s.tags) + "' placeholder='temperatura,humedad'>";
      html += "<label>Payload</label>";
      html += "<textarea name='dht_payload' rows='3'>" + esc(s.payload == "" ? String("{valor}") : s.payload) + "</textarea>";
      html += "<p class='hint'>Placeholders: <code>{valor}</code> (número), <code>{dato}</code> (temperatura o humedad) y <code>{unidad}</code> (°C o %). Con etiquetas, el resultado debe ser JSON válido.</p>";

      html += "<label>Mensajes que se enviarán (última lectura, según lo guardado)</label>";
      if (sensorDHT != nullptr && dhtUltimoEstadoOK) {
        html += "<pre class='code'>" + esc(base + "/temperatura") + "\n" + esc(construirPayloadDHT(s, "temperatura", "°C", dhtUltimaTemp));
        html += "\n\n" + esc(base + "/humedad") + "\n" + esc(construirPayloadDHT(s, "humedad", "%", dhtUltimaHum)) + "</pre>";
      } else {
        html += "<p class='hint'>Todavía no hay una lectura válida del sensor.</p>";
      }
    }

    // Prueba en vivo: confirma que el pin/modelo elegido corresponde al
    // sensor conectado, sin exponer datos concretos de lectura.
    html += "<button type='submit' name='probar_id' value='" + s.id + "' formaction='/probar-sensor' class='secundario'>" + String(s.tipo == "mqtt" ? "Probar publicación" : (s.id == "dht" ? "Probar sensor" : "Probar conexión")) + "</button>";
    if (s.id == "dht") {
      html += "<button type='submit' name='probar_id' value='dht_pub' formaction='/probar-sensor' class='secundario'>Probar publicación</button>";
    }

    if (ultimoIdProbado == s.id && ultimoResultadoPrueba != "") {
      html += "<p style='font-size:12px;margin-top:8px;color:#475569;'>" + ultimoResultadoPrueba + "</p>";
    }

    html += "</div>";
  }

  html += "<button type='submit'>Guardar</button>";
  html += "</form>";
  html += "</div>";
  html += htmlFooter();
  server.send(200, "text/html", html);

  // El mensaje de prueba se muestra una sola vez, después se limpia
  ultimoIdProbado = "";
  ultimoResultadoPrueba = "";
}

// Prueba en vivo el pin/modelo elegido SIN guardar la configuración,
// para confirmar que el sensor conectado corresponde a lo seleccionado.
// Solo informa si hay conexión o no (pin y modelo correctos), no expone
// valores de lectura concretos.
void handleProbarSensor() {
  String id = server.arg("probar_id");
  ultimoIdProbado = id;

  if (id == "dht") {
    int pin = server.hasArg("dht_p0") ? server.arg("dht_p0").toInt() : -1;
    String modelo = server.hasArg("dht_opt") ? server.arg("dht_opt") : "DHT22";

    if (pin == -1) {
      ultimoResultadoPrueba = "Seleccioná un pin antes de probar.";
    } else {
      uint8_t tipo = (modelo == "DHT11") ? DHT11 : DHT22;
      DHT dhtPrueba(pin, tipo);
      dhtPrueba.begin();
      delay(2200); // el DHT necesita ~2s desde begin() para dar una lectura válida
      float t = dhtPrueba.readTemperature();
      float h = dhtPrueba.readHumidity();
      if (isnan(t) || isnan(h)) {
        ultimoResultadoPrueba = "&#10007; Sin conexión en GPIO " + String(pin) + ". Revisá el pin y el modelo (" + modelo + ").";
      } else {
        ultimoResultadoPrueba = "&#10003; Conexión establecida en GPIO " + String(pin) + " (" + modelo + ").";
      }
    }
  } else if (id == "dht_pub") {
    // Publica la última lectura con los valores del formulario (sin guardarlos)
    ultimoIdProbado = "dht";
    String base = server.arg("dht_canal");
    base.trim();
    while (base.endsWith("/")) base.remove(base.length() - 1);
    if (base == "") base = canalBaseDHTPorDefecto();

    if (!mqttHabilitado) {
      ultimoResultadoPrueba = "&#10007; El MQTT está deshabilitado. Activalo en Conexiones.";
    } else if (!mqttClient.connected()) {
      ultimoResultadoPrueba = "&#10007; Sin conexión con el broker.";
      if (mqttUltimoError != "") ultimoResultadoPrueba += " " + esc(mqttUltimoError);
    } else if (sensorDHT == nullptr || !dhtUltimoEstadoOK) {
      ultimoResultadoPrueba = "&#10007; No hay una lectura válida del sensor. Probá primero el sensor (pin y modelo).";
    } else {
      Servicio prueba = servicios[indiceServicioDHT];
      prueba.tags = server.arg("dht_tags");
      prueba.tags.trim();
      prueba.payload = server.arg("dht_payload");

      String tTemp = base + "/temperatura";
      String tHum  = base + "/humedad";
      String pTemp = construirPayloadDHT(prueba, "temperatura", "°C", dhtUltimaTemp);
      String pHum  = construirPayloadDHT(prueba, "humedad", "%", dhtUltimaHum);
      bool okT = mqttClient.publish(tTemp.c_str(), pTemp.c_str());
      bool okH = mqttClient.publish(tHum.c_str(), pHum.c_str());

      ultimoResultadoPrueba = String(okT ? "&#10003; " : "&#10007; ") + esc(tTemp) + ": " + esc(pTemp) + "<br>";
      ultimoResultadoPrueba += String(okH ? "&#10003; " : "&#10007; ") + esc(tHum) + ": " + esc(pHum);
    }
  } else if (id == "mqtt") {
    // Publica una vez con los valores del formulario (sin guardarlos)
    String canal = server.arg("mqtt_canal");
    canal.trim();

    if (canal == "") {
      ultimoResultadoPrueba = "&#10007; Escribí un canal antes de probar.";
    } else if (!mqttHabilitado) {
      ultimoResultadoPrueba = "&#10007; El MQTT está deshabilitado. Activalo en Conexiones.";
    } else if (!mqttClient.connected()) {
      ultimoResultadoPrueba = "&#10007; Sin conexión con el broker.";
      if (mqttUltimoError != "") ultimoResultadoPrueba += " " + esc(mqttUltimoError);
    } else {
      Servicio prueba = servicios[indiceServicioMQTT];
      prueba.canal = canal;
      prueba.tags = server.arg("mqtt_tags");
      prueba.tags.trim();
      prueba.payload = server.arg("mqtt_payload");
      String payload = construirPayloadMQTT(prueba);

      if (mqttClient.publish(canal.c_str(), payload.c_str())) {
        ultimoResultadoPrueba = "&#10003; Publicado en <b>" + esc(canal) + "</b>: " + esc(payload);
      } else {
        ultimoResultadoPrueba = "&#10007; Falló la publicación en <b>" + esc(canal) + "</b> (canal inválido, payload demasiado grande o conexión caída).";
      }
    }
  } else {
    ultimoResultadoPrueba = "Servicio no reconocido.";
  }

  server.sendHeader("Location", "/sensores", true);
  server.send(302, "text/plain", "");
}

void handleGuardarSensores() {
  for (int i = 0; i < cantidadServicios; i++) {
    Servicio& s = servicios[i];

    s.habilitado = server.hasArg(s.id + "_hab");

    for (int p = 0; p < s.cantidadPines; p++) {
      String nombreCampo = s.id + "_p" + String(p);
      if (server.hasArg(nombreCampo)) {
        s.pines[p] = server.arg(nombreCampo).toInt();
      }
    }

    if (s.tieneOpcion) {
      String nombreCampoOpt = s.id + "_opt";
      if (server.hasArg(nombreCampoOpt)) {
        s.opcionSeleccionada = server.arg(nombreCampoOpt);
      }
    }

    if (s.tipo == "mqtt" || s.id == "dht") {
      if (server.hasArg(s.id + "_canal")) s.canal = server.arg(s.id + "_canal");
      if (server.hasArg(s.id + "_tags")) s.tags = server.arg(s.id + "_tags");
      if (server.hasArg(s.id + "_payload")) s.payload = server.arg(s.id + "_payload");
      s.canal.trim();
      s.tags.trim();

      if (s.id == "dht") {
        while (s.canal.endsWith("/")) s.canal.remove(s.canal.length() - 1);
        if (s.canal == canalBaseDHTPorDefecto()) s.canal = "";   // vacío = sigue raíz/módulo
        if (s.payload == "") s.payload = "{valor}";
      }
    }

    guardarConfigServicio(i);
  }

  reinicializarSensorDHT();
  actualizarEstadoSensores();

  server.sendHeader("Location", "/sensores", true);
  server.send(302, "text/plain", "");
}

// =====================================================================
// PÁGINA: RED (/red)
// =====================================================================

void handleRed() {
  String html = htmlHeader("BioRed - Red");
  html += navBar("red");
  html += "<div class='container'>";
  html += "<h1>Red</h1>";
  html += "<p class='sub'>Elegí tu red WiFi</p>";

  bool activa = internetActivo();
  html += "<div class='row'><span class='label'>Estado</span>";
  html += activa ? "<span class='badge badge-activa'>Conectado</span>" : "<span class='badge badge-inactiva'>Desconectado</span>";
  html += "</div>";
  if (activa) {
    html += "<div class='row'><span class='label'>Red actual</span><span>" + WiFi.SSID() + "</span></div>";
    html += "<div class='row'><span class='label'>Señal</span><span>" + String(WiFi.RSSI()) + " dBm (" + calidadSenal(WiFi.RSSI()) + ")</span></div>";
  }

  html += "<form action='/conectar' method='POST'>";
  html += "<label>Seleccionar red</label>";
  html += "<select name='ssid'>";
  if (cantidadRedes == 0) {
    html += "<option value=''>No se encontraron redes</option>";
  } else {
    for (int i = 0; i < cantidadRedes; i++) {
      html += "<option value='" + redesSSID[i] + "'>";
      html += redesSSID[i] + " (" + String(redesRSSI[i]) + " dBm)";
      html += "</option>";
    }
  }
  html += "</select>";
  html += "<label>Contraseña</label>";
  html += "<input type='password' name='password' placeholder='Contraseña de la red'>";
  html += "<button type='submit'>Conectar</button>";
  html += "</form>";
  html += "<a class='link' href='/rescan'>&#8635; Actualizar lista de redes</a>";

  html += "</div>";
  html += htmlFooter();
  server.send(200, "text/html", html);
}

void handleRescan() {
  escanearRedesUnaVez();
  server.sendHeader("Location", "/red", true);
  server.send(302, "text/plain", "");
}

// ---------- Confirmación de conexión (éxito / error) ----------

void handleSuccessPage() {
  String html = htmlHeader("Conexión exitosa");
  html += navBar("red");
  html += "<div class='container'>";
  html += "<div class='status-icon ok'>&#10003;</div>";
  html += "<h1 style='text-align:center'>¡Conectado con éxito!</h1>";
  html += "<p class='sub' style='text-align:center'>El ESP32 se conectó correctamente</p>";
  html += "<div class='row'><span class='label'>Red</span><span>" + WiFi.SSID() + "</span></div>";
  html += "<div class='row'><span class='label'>IP</span><span>" + WiFi.localIP().toString() + "</span></div>";
  html += "<div class='row'><span class='label'>Señal</span><span>" + String(WiFi.RSSI()) + " dBm (" + calidadSenal(WiFi.RSSI()) + ")</span></div>";
  html += "<a class='link' href='/red'>&larr; Volver</a>";
  html += "</div>";
  html += htmlFooter();
  server.send(200, "text/html", html);
}

void handleFailPage(const String& motivo) {
  String html = htmlHeader("Error de conexión");
  html += navBar("red");
  html += "<div class='container'>";
  html += "<div class='status-icon fail'>&#10007;</div>";
  html += "<h1 style='text-align:center'>No se pudo conectar</h1>";
  html += "<p class='sub' style='text-align:center'>" + motivo + "</p>";
  html += "<a class='link' href='/red'>&larr; Volver / Intentar de nuevo</a>";
  html += "</div>";
  html += htmlFooter();
  server.send(200, "text/html", html);
}

void handleConectar() {
  if (!server.hasArg("ssid") || server.arg("ssid") == "") {
    handleFailPage("No se seleccionó ninguna red.");
    return;
  }

  String ssid = server.arg("ssid");
  String password = server.arg("password");

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long inicio = millis();
  wl_status_t estado;
  while ((estado = WiFi.status()) != WL_CONNECTED && millis() - inicio < CONNECT_TIMEOUT_MS) {
    delay(300);
  }

  if (estado == WL_CONNECTED) {
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("password", password);
    prefs.end();
    handleSuccessPage();
  } else {
    String motivo;
    switch (estado) {
      case WL_NO_SSID_AVAIL:
        motivo = "La red seleccionada no está disponible.";
        break;
      case WL_CONNECT_FAILED:
        motivo = "Contraseña incorrecta o fallo de autenticación.";
        break;
      default:
        motivo = "Tiempo de espera agotado. Verificá la señal o la contraseña.";
        break;
    }
    WiFi.disconnect();
    escanearRedesUnaVez();
    handleFailPage(motivo);
  }
}

// =====================================================================
// PÁGINA: CONEXIONES / MQTT (/conexiones)
// =====================================================================

void handleConexiones() {
  String html = htmlHeader("BioRed - Conexiones");
  html += navBar("conexiones");
  html += "<div class='container'>";
  html += "<h1>Conexiones</h1>";
  html += "<p class='sub'>Configurá el broker MQTT</p>";

  html += "<div class='row'><span class='label'>Estado MQTT</span>";
  if (!mqttHabilitado) {
    html += "<span class='badge badge-inactiva'>Deshabilitado</span>";
  } else {
    html += mqttClient.connected()
              ? "<span class='badge badge-activa'>Conectado</span>"
              : "<span class='badge badge-inactiva'>Desconectado</span>";
  }
  html += "</div>";

  // Último error de la conexión "real" (la que corre en segundo plano)
  html += htmlAlertaErrorMQTT();

  if (mqttHabilitado && mqttClient.connected() && mqttUltimoMensaje != "") {
    html += "<div class='row'><span class='label'>Último mensaje</span><span style='font-size:12px;text-align:right;'>" + esc(mqttUltimoMensaje) + "</span></div>";
  }

  // Resultado de la prueba manual (se muestra una sola vez)
  if (mqttResultadoPrueba != "") {
    html += "<div class='alert " + String(mqttResultadoPruebaOK ? "alert-ok" : "alert-error") + "'>";
    html += "<strong>Resultado de la prueba</strong><br>" + mqttResultadoPrueba + "</div>";
    mqttResultadoPrueba = "";
  }

  html += "<form action='/guardar-conexiones' method='POST'>";
  html += "<div class='toggle-row' style='margin-top:14px;'><strong>Habilitar MQTT</strong>";
  html += "<label class='switch'><input type='checkbox' name='on'";
  if (mqttHabilitado) html += " checked";
  html += "><span class='slider'></span></label></div>";

  html += "<label>Host / IP del broker</label>";
  html += "<input type='text' name='host' placeholder='ej: 192.168.1.50' value='" + esc(mqttHost) + "'>";
  html += "<label>Puerto</label>";
  html += "<input type='number' name='port' value='" + String(mqttPort) + "'>";
  html += "<label>Raíz del proyecto</label>";
  html += "<input type='text' name='raiz' placeholder='" + String(MQTT_RAIZ_DEFAULT) + "' value='" + esc(mqttRaiz) + "'>";
  html += "<label>ID de este módulo</label>";
  html += "<input type='text' name='modulo' placeholder='" + String(MQTT_MODULO_DEFAULT) + "' value='" + esc(mqttModulo) + "'>";
  html += "<p class='hint'>Los topics se arman como raíz/módulo/dato. Solo minúsculas, números, - y _.</p>";
  html += "<label>Topic de prueba</label>";
  html += "<input type='text' name='topic' placeholder='" + esc(topicPruebaPorDefecto()) + "' value='" + esc(mqttTopicPrueba) + "'>";
  html += "<p class='hint'>Vacío = " + esc(topicPruebaPorDefecto()) + ". Cambialo (ej. con <code>#</code> o muy largo) para provocar errores y verlos acá.</p>";

  html += "<p class='hint' style='margin-top:14px;'>Topics de este módulo (guardados):</p>";
  html += "<div class='row'><span class='label'>Pruebas</span><span style='font-size:12px;'>" + esc(mqttTopicPrueba) + "</span></div>";
  html += "<div class='row'><span class='label'>Temperatura</span><span style='font-size:12px;'>" + esc(canalBaseDHT() + "/temperatura") + "</span></div>";
  html += "<div class='row'><span class='label'>Humedad</span><span style='font-size:12px;'>" + esc(canalBaseDHT() + "/humedad") + "</span></div>";

  html += "<button type='submit'>Guardar y conectar</button>";
  html += "<button type='submit' formaction='/probar-mqtt' class='secundario'>Probar conexión (sin guardar)</button>";
  html += "</form>";
  html += "<a class='link' href='/conexiones'>&#8635; Actualizar estado</a>";

  html += "</div>";
  html += htmlFooter();
  server.send(200, "text/html", html);
}

void handleGuardarConexiones() {
  mqttHost = server.arg("host");
  mqttHost.trim();
  mqttPort = server.arg("port").toInt();
  if (mqttPort <= 0) mqttPort = 1883;
  mqttHabilitado = server.hasArg("on");

  mqttRaiz = sanitizarSegmento(server.arg("raiz"), MQTT_RAIZ_DEFAULT);
  mqttModulo = sanitizarSegmento(server.arg("modulo"), MQTT_MODULO_DEFAULT);

  mqttTopicPrueba = server.arg("topic");
  mqttTopicPrueba.trim();
  if (mqttTopicPrueba == "") mqttTopicPrueba = topicPruebaPorDefecto();

  guardarConfigMQTT();
  aplicarConfigMQTT();

  server.sendHeader("Location", "/conexiones", true);
  server.send(302, "text/plain", "");
}

// Callback del cliente de prueba: solo marca que el mensaje volvió.
void mqttCallbackPrueba(char* topic, byte* payload, unsigned int length) {
  mqttEcoRecibido = true;
}

// Prueba paso a paso (conectar -> suscribir -> publicar -> eco) con los
// valores del formulario, SIN guardarlos ni tocar la conexión real.
void handleProbarMQTT() {
  String host = server.arg("host");
  host.trim();
  int port = server.arg("port").toInt();
  if (port <= 0) port = 1883;
  String topic = server.arg("topic");
  topic.trim();
  if (topic == "") topic = sanitizarSegmento(server.arg("raiz"), MQTT_RAIZ_DEFAULT) + "/lab/hello";

  String r = "";
  bool ok = false;
  String motivo;

  if (!verificarPrerequisitosMQTT(host, motivo)) {
    r = "&#10007; " + esc(motivo);
  } else {
    WiFiClient clientePrueba;
    PubSubClient t(clientePrueba);
    t.setServer(host.c_str(), port);
    t.setSocketTimeout(5);
    t.setCallback(mqttCallbackPrueba);

    String cid = "ESP32-test-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
    unsigned long t0 = millis();

    if (!t.connect(cid.c_str())) {
      int rc = t.state();
      r = "&#10007; Conexión fallida a " + esc(host) + ":" + String(port) + " (rc=" + String(rc) + ")<br>" + esc(descripcionErrorMQTT(rc));
    } else {
      r = "&#10003; Conexión al broker OK (" + String(millis() - t0) + " ms)<br>";

      mqttEcoRecibido = false;
      bool subOk = t.subscribe(topic.c_str());
      r += String(subOk ? "&#10003;" : "&#10007;") + " Suscripción a <b>" + esc(topic) + "</b><br>";

      bool pubOk = t.publish(topic.c_str(), "hello");
      r += String(pubOk ? "&#10003;" : "&#10007;") + " Publicación de &quot;hello&quot;";
      if (!pubOk) r += " (topic demasiado largo o conexión caída)";
      r += "<br>";

      if (pubOk) {
        unsigned long espera = millis();
        while (!mqttEcoRecibido && millis() - espera < 1500) {
          t.loop();
          delay(20);
        }
        r += mqttEcoRecibido
               ? "&#10003; Mensaje recibido de vuelta (eco)"
               : "&#9888; No llegó el eco: el broker puede estar bloqueando lectura/escritura en ese topic (ACL)";
      }

      ok = subOk && pubOk;
      t.disconnect();
    }
  }

  Serial.println("[MQTT] Prueba manual: " + String(ok ? "OK" : "con errores"));
  mqttResultadoPrueba = r;
  mqttResultadoPruebaOK = ok;

  server.sendHeader("Location", "/conexiones", true);
  server.send(302, "text/plain", "");
}

// =====================================================================
// PÁGINA: MEMORIA (/memoria)
// =====================================================================

void handleMemoria() {
  String html = htmlHeader("BioRed - Memoria");
  html += navBar("");
  html += "<div class='container'>";
  html += "<h1>Memoria del ESP32</h1>";
  html += "<p class='sub'>Estado actual de RAM</p>";

  // El WROOM-32 no tiene PSRAM física, por eso se retiraron esas métricas
  // y se agregaron otras relevantes para monitorear la salud de la RAM.
  html += "<div class='row'><span class='label'>RAM libre</span><span>" + String(ESP.getFreeHeap()) + " bytes</span></div>";
  html += "<div class='row'><span class='label'>RAM total</span><span>" + String(ESP.getHeapSize()) + " bytes</span></div>";
  html += "<div class='row'><span class='label'>Mínimo histórico libre</span><span>" + String(ESP.getMinFreeHeap()) + " bytes</span></div>";
  html += "<div class='row'><span class='label'>Mayor bloque asignable</span><span>" + String(ESP.getMaxAllocHeap()) + " bytes</span></div>";
  html += "<div class='row'><span class='label'>Tiempo encendido</span><span>" + String(millis() / 1000) + " s</span></div>";

  html += "<a class='link' href='/'>&larr; Volver</a>";
  html += "</div>";
  html += htmlFooter();
  server.send(200, "text/html", html);
}

// =====================================================================
// PORTAL CAUTIVO / RUTAS DESCONOCIDAS
// =====================================================================

void handleNotFound() {
  Serial.println("[HTTP] No encontrada: " + server.uri() + " de " + server.client().remoteIP().toString());
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// =====================================================================
// CONEXIÓN AUTOMÁTICA CON CREDENCIALES GUARDADAS
// =====================================================================

bool intentarConexionGuardada() {
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String password = prefs.getString("password", "");
  prefs.end();

  if (ssid == "") return false;

  Serial.println("Intentando conectar con credenciales guardadas: " + ssid);
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < CONNECT_TIMEOUT_MS) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  return WiFi.status() == WL_CONNECTED;
}

// =====================================================================
// SETUP / LOOP
// =====================================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  inicializarServiciosDisponibles();
  cargarConfigServicios();
  reinicializarSensorDHT();
  actualizarEstadoSensores();

  cargarConfigMQTT();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress apIP = WiFi.softAPIP();
  Serial.println("AP iniciado. Conectate a la red '" + String(AP_SSID) + "' e ingresá a http://" + apIP.toString());
  dnsServer.start(DNS_PORT, "*", apIP);

  if (intentarConexionGuardada()) {
    Serial.println("Conectado. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("No se pudo conectar automáticamente. Esperando configuración...");
  }

  // El escaneo se hace siempre (haya conectado o no), para que /red
  // ya tenga la lista lista sin tener que esperar al primer clic
  escanearRedesUnaVez();

  if (mqttHabilitado) {
    aplicarConfigMQTT();
  }

  server.on("/", HTTP_GET, handleHome);
  server.on("/sensores", HTTP_GET, handleSensores);
  server.on("/guardar-sensores", HTTP_POST, handleGuardarSensores);
  server.on("/probar-sensor", HTTP_POST, handleProbarSensor);
  server.on("/red", HTTP_GET, handleRed);
  server.on("/rescan", HTTP_GET, handleRescan);
  server.on("/conectar", HTTP_POST, handleConectar);
  server.on("/conexiones", HTTP_GET, handleConexiones);
  server.on("/guardar-conexiones", HTTP_POST, handleGuardarConexiones);
  server.on("/probar-mqtt", HTTP_POST, handleProbarMQTT);
  server.on("/memoria", HTTP_GET, handleMemoria);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[HTTP] Servidor web iniciado en el puerto 80");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  if (mqttHabilitado) {
    if (mqttClient.connected()) {
      mqttClient.loop();
      publicarServicioMQTT();
      publicarSensoresMQTT();
    } else if (millis() - ultimoIntentoMQTT > MQTT_RECONNECT_INTERVAL) {
      ultimoIntentoMQTT = millis();
      intentarConectarMQTT();
    }
  }

  // Actualiza el estado cacheado de sensores (usado en Home) sin bloquear
  // ninguna respuesta HTTP con una lectura directa del sensor.
  if (millis() - ultimaLecturaEstadoDHT > INTERVALO_LECTURA_ESTADO) {
    ultimaLecturaEstadoDHT = millis();
    actualizarEstadoSensores();
  }
}
