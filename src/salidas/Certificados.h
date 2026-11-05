#pragma once
// Raíces de confianza para validar el HTTPS de la API (no se usa setInsecure).
//
// PEGA AQUÍ, sin cambios, el bloque CERTIFICADOS_RAIZ de tu antiguo EnvioAPI.cpp
// (las raíces de Google Trust Services, Let's Encrypt y SSL.com), así:
//
//   const char CERTIFICADOS_RAIZ[] = R"PEM(
//   -----BEGIN CERTIFICATE-----
//   ...
//   -----END CERTIFICATE-----
//   )PEM";
//
// Después borra la línea #error.
#error "Falta pegar CERTIFICADOS_RAIZ en src/Certificados.h (ver comentario)"
