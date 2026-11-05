#pragma once
// Cámara opcional con pines definidos para la Freenove ESP32-S3-WROOM CAM.
bool inicializarCamara();            // false si falla: el resto del firmware sigue funcionando
void enviarFotoCamaraBajaResolucion();
void enviarFotoCamaraAltaResolucion();
void transmitirVideoCamara();
