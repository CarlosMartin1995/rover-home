#pragma once

// ============================================================
//  webserver.h - Access Point WiFi + servidor HTTP + pagina
//  ------------------------------------------------------------
//  Sirve la pagina de control (D-pad) y traduce los endpoints a
//  llamadas al modulo motor. La logica de movimiento vive en
//  motor.*; este modulo solo es la capa de comunicacion.
// ============================================================

// Levanta el AP (Rover_IA), registra las rutas HTTP y arranca el
// servidor. Llamar una vez en setup(), despues de motorsBegin().
void webServerBegin();

// Atiende clientes HTTP. Llamar en cada loop().
void webServerLoop();
