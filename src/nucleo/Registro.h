#pragma once
#include <ArduinoJson.h>
#include "../sensores/Sensor.h"

// Registro de sensores. Crea los que estén compilados, arranca los activados
// en el portal y serializa el acceso (loop() escribe, las tareas de envío leen).
void    registroInit();
void    registroActualizar();                 // llamar en loop()
int     registroCantidad();
Sensor* registroSensor(int i);
bool    registroActivo(int i);                // estado en ejecución
int     registroPin(int i);
void    registroConfigurar(const char* id, bool on, int pin);   // guarda; aplica al reiniciar
int     registroDatos(int i, Dato* out, int max);
bool    registroArmarApi(JsonDocument& doc);  // añade las secciones de los sensores activos
