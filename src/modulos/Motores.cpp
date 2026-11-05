#include "../BioRedConfig.h"
#if BIORED_MODULO_MOTORES
#include "Motores.h"
#include <Stepper.h>

namespace {
  const int PASOS_POR_REVOLUCION = 2048;
  const int VELOCIDAD_RPM        = 5;
  const int PASOS_POR_PULSO      = 20;

  Stepper motorPan(PASOS_POR_REVOLUCION, 48, 21, 47, 20);
  Stepper motorTilt(PASOS_POR_REVOLUCION, 1, 42, 2, 41);
}

void motoresInit() {
  motorPan.setSpeed(VELOCIDAD_RPM);
  motorTilt.setSpeed(VELOCIDAD_RPM);
}

int motoresPasosPorDefecto() { return PASOS_POR_PULSO; }

// Las bobinas quedan energizadas al terminar para mantener el par de sujeción.
bool motoresMover(const String& dir, int& pasos, String& error) {
  pasos = constrain(pasos, 1, 200);
  if      (dir == "ARRIBA") motorTilt.step( pasos);
  else if (dir == "ABAJO")  motorTilt.step(-pasos);
  else if (dir == "IZQ")    motorPan.step( pasos);
  else if (dir == "DER")    motorPan.step(-pasos);
  else { error = "Dirección inválida. Usa ARRIBA, ABAJO, IZQ o DER"; return false; }
  Serial.printf("[Motor] %s %d pasos\n", dir.c_str(), pasos);
  return true;
}
#endif
