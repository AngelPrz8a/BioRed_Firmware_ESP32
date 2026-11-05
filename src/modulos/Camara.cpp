#include "../BioRedConfig.h"
#if BIORED_MODULO_CAMARA
#include "Camara.h"
#include <WiFi.h>
#include <WebServer.h>
#include <esp32cam.h>
#include <esp_camera.h>

extern WebServer server;   // definido en Portal.cpp

namespace {
  const esp32cam::Pins FreeNoveS3{
    /*D0*/ 11, /*D1*/ 9, /*D2*/ 8, /*D3*/ 10,
    /*D4*/ 12, /*D5*/ 18, /*D6*/ 17, /*D7*/ 16,
    /*XCLK*/ 15, /*PCLK*/ 13, /*VSYNC*/ 6, /*HREF*/ 7,
    /*SDA*/ 4, /*SCL*/ 5, /*RESET*/ -1, /*PWDN*/ -1
  };

  auto camLoRes     = esp32cam::Resolution::find(320, 240);
  auto camHiRes     = esp32cam::Resolution::find(800, 600);
  auto camStreamRes = esp32cam::Resolution::find(320, 240);
  esp32cam::Resolution camResActual = camHiRes;
  bool camOk = false;

  void ajustarResolucion(const esp32cam::Resolution& res) {
    if (res == camResActual) return;
    if (!esp32cam::Camera.changeResolution(res)) Serial.println("[Camara] SET-RES FAIL");
    camResActual = res;
  }

  void serveJpg(const esp32cam::Resolution& res) {
    if (!camOk) { server.send(503, "text/plain", "Camara no disponible"); return; }
    ajustarResolucion(res);
    auto frame = esp32cam::capture();
    if (frame == nullptr) { Serial.println("[Camara] CAPTURE FAIL"); server.send(503, "text/plain", "Sin imagen"); return; }
    server.setContentLength(frame->size());
    server.send(200, "image/jpeg");
    WiFiClient client = server.client();
    frame->writeTo(client);
  }
}

bool camaraInit() {
  Serial.printf("[Camara] PSRAM: %s (%u bytes)\n", psramFound() ? "OK" : "NO", (unsigned)ESP.getPsramSize());
  esp32cam::setLogger(Serial);
  using namespace esp32cam;
  Config c;
  c.setPins(FreeNoveS3);
  c.setResolution(camHiRes);
  c.setBufferCount(2);
  c.setJpeg(80);
  camOk = Camera.begin(c);
  Serial.println(camOk ? "[Camara] OK" : "[Camara] FAIL: verificar pines y Tools > PSRAM = OPI PSRAM");
  return camOk;   // antes se quedaba bloqueado; ahora el portal sigue disponible
}

void camaraHandleJpgLo() { serveJpg(camLoRes); }
void camaraHandleJpgHi() { serveJpg(camHiRes); }

void camaraHandleStream() {
  if (!camOk) { server.send(503, "text/plain", "Camara no disponible"); return; }
  WiFiClient client = server.client();
  ajustarResolucion(camStreamRes);
  client.print("HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=frame\r\n"
               "Cache-Control: no-cache\r\nConnection: close\r\n\r\n");
  int sinFrame = 0;
  while (client.connected()) {
    auto frame = esp32cam::capture();
    if (frame == nullptr) { if (++sinFrame > 100) break; delay(10); continue; }
    sinFrame = 0;
    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", (unsigned)frame->size());
    if (!frame->writeTo(client)) break;
    client.print("\r\n");
  }
  Serial.println("[Camara] Stream cerrado");
}
#endif
