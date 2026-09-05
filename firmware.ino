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
const char* AP_PASSWORD = "12345678"; // mínimo 8 caracteres, o "" para AP abierto
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
  bool   habilitado;
  int    cantidadPines;
  String nombresPines[MAX_PINES_SERVICIO];
  int    pines[MAX_PINES_SERVICIO];       // -1 = sin asignar
  bool   tieneOpcion;
  String nombreOpcion;
  String opciones[MAX_OPCIONES];
  int    cantidadOpciones;
  String opcionSeleccionada;
};

Servicio servicios[MAX_SERVICIOS];
int cantidadServicios = 0;
int indiceServicioDHT = -1;
String ultimoIdProbado = "";      // último servicio testeado desde /sensores
String ultimoResultadoPrueba = ""; // resultado a mostrar una sola vez

// Pines GPIO recomendados para uso general en ESP32 WROOM-32
const int PINES_DISPONIBLES[] = {4, 5, 12, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
const int CANT_PINES_DISPONIBLES = sizeof(PINES_DISPONIBLES) / sizeof(PINES_DISPONIBLES[0]);

DHT* sensorDHT = nullptr;

// Estado cacheado de sensores para Home (no se lee el sensor en cada request)
bool dhtUltimoEstadoOK = false;
unsigned long ultimaLecturaEstadoDHT = 0;
const unsigned long INTERVALO_LECTURA_ESTADO = 10000; // 10s

void inicializarServiciosDisponibles() {
  cantidadServicios = 0;

  Servicio& dht = servicios[cantidadServicios];
  dht.id = "dht";
  dht.nombre = "DHT (temperatura/humedad)";
  dht.habilitado = false;
  dht.cantidadPines = 1;
  dht.nombresPines[0] = "Pin de datos";
  dht.pines[0] = -1;
  dht.tieneOpcion = true;
  dht.nombreOpcion = "Modelo";
  dht.opciones[0] = "DHT11";
  dht.opciones[1] = "DHT22";
  dht.cantidadOpciones = 2;
  dht.opcionSeleccionada = "DHT22";
  indiceServicioDHT = cantidadServicios;
  cantidadServicios++;

  // Para agregar un servicio nuevo, copiar el bloque de arriba
  // y ajustar id, nombre, cantidadPines, nombresPines y opciones.
  // No olvidar agregar también su caso correspondiente en handleProbarSensor().
}

void cargarConfigServicios() {
  prefs.begin("config", true);
  for (int i = 0; i < cantidadServicios; i++) {
    servicios[i].habilitado = prefs.getBool((servicios[i].id + "_on").c_str(), false);
    for (int p = 0; p < servicios[i].cantidadPines; p++) {
      String key = servicios[i].id + "_p" + String(p);
      servicios[i].pines[p] = prefs.getInt(key.c_str(), -1);
    }
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
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;

void cargarConfigMQTT() {
  prefs.begin("mqtt", true);
  mqttHost = prefs.getString("host", "");
  mqttPort = prefs.getInt("port", 1883);
  mqttHabilitado = prefs.getBool("on", false);
  prefs.end();
}

void guardarConfigMQTT() {
  prefs.begin("mqtt", false);
  prefs.putString("host", mqttHost);
  prefs.putInt("port", mqttPort);
  prefs.putBool("on", mqttHabilitado);
  prefs.end();
}

void intentarConectarMQTT() {
  if (!mqttHabilitado || mqttHost == "" || !internetActivo()) return;
  if (mqttClient.connected()) return;

  String clientId = "ESP32-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
  Serial.println("[MQTT] Conectando a " + mqttHost + ":" + String(mqttPort) + " como " + clientId);

  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("[MQTT] Conectado");
  } else {
    Serial.println("[MQTT] Fallo al conectar, rc=" + String(mqttClient.state()));
  }
}

void aplicarConfigMQTT() {
  mqttClient.setServer(mqttHost.c_str(), mqttPort);
  if (mqttClient.connected()) mqttClient.disconnect();
  intentarConectarMQTT();
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
  select, input[type=password], input[type=text], input[type=number] {
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
</style>
</head>
<body>
)rawliteral";
  return html;
}

String htmlFooter() {
  return "</body></html>";
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

    // Prueba en vivo: confirma que el pin/modelo elegido corresponde al
    // sensor conectado, sin exponer datos concretos de lectura.
    html += "<button type='submit' name='probar_id' value='" + s.id + "' formaction='/probar-sensor' class='secundario'>Probar conexión</button>";

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

  html += "<form action='/guardar-conexiones' method='POST'>";
  html += "<div class='toggle-row' style='margin-top:14px;'><strong>Habilitar MQTT</strong>";
  html += "<label class='switch'><input type='checkbox' name='on'";
  if (mqttHabilitado) html += " checked";
  html += "><span class='slider'></span></label></div>";

  html += "<label>Host / IP del broker</label>";
  html += "<input type='text' name='host' placeholder='ej: 192.168.1.50' value='" + mqttHost + "'>";
  html += "<label>Puerto</label>";
  html += "<input type='number' name='port' value='" + String(mqttPort) + "'>";
  html += "<button type='submit'>Guardar y conectar</button>";
  html += "</form>";

  html += "</div>";
  html += htmlFooter();
  server.send(200, "text/html", html);
}

void handleGuardarConexiones() {
  mqttHost = server.arg("host");
  mqttPort = server.arg("port").toInt();
  if (mqttPort <= 0) mqttPort = 1883;
  mqttHabilitado = server.hasArg("on");

  guardarConfigMQTT();
  aplicarConfigMQTT();

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
