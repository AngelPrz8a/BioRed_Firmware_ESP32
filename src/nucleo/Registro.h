#pragma once
#include <ArduinoJson.h>
#include "../sensores/Sensor.h"

// Registro de sensores. Crea los que estén compilados, arranca los activados
// en el portal y serializa el acceso (loop() escribe, las tareas de envío leen).
void iniciarRegistroSensores();
void actualizarRegistroSensores();  // Llamar desde loop().
int cantidadSensoresRegistrados();
Sensor* obtenerSensorRegistrado(int indice);
bool sensorRegistradoActivo(int indice);
int pinSensorRegistrado(int indice);
void configurarSensorRegistrado(const char* identificadorSensor, bool activo, int pin);
int obtenerLecturasSensor(int indice, LecturaSensor* lecturas, int capacidad);
bool agregarSensoresAlJsonApi(JsonDocument& documento);
