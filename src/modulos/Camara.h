#pragma once
// Cámara FreeNove ESP32-S3-WROOM CAM (OV2640/OV3660/OV5640). Módulo opcional.
bool camaraInit();            // false si falla: el resto del firmware sigue funcionando
void camaraHandleJpgLo();
void camaraHandleJpgHi();
void camaraHandleStream();
