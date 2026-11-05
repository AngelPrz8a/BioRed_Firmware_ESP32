#pragma once
#include <Arduino.h>

// Motores paso a paso pan/tilt. Módulo opcional.
void inicializarMotores();
// dir: "ARRIBA"|"ABAJO"|"IZQ"|"DER". pasos queda ajustado a 1-200.
bool moverMotor(const String& dir, int& pasos, String& error);
int  pasosPredeterminadosMotor();
