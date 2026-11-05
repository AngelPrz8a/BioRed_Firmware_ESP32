#pragma once
// Cámara opcional con pines definidos para la Freenove ESP32-S3-WROOM CAM.
bool camaraInit();            // false si falla: el resto del firmware sigue funcionando
void camaraHandleJpgLo();
void camaraHandleJpgHi();
void camaraHandleStream();
