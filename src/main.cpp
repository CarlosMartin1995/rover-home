/*
  ============================================================
  ROVER 4WD - Firmware ESP32-S3  (v2 / modular, PlatformIO)
  ------------------------------------------------------------
  Misma logica y reglas de seguridad que el monolitico de
  referencia (reference/rover_wifi_ap_v1_tb6612.ino), separada
  en modulos:
    config.h     -> pinout, constantes, credenciales AP
    motor.*      -> abstraccion TB6612FNG + watchdog
    webserver.*  -> Access Point + HTTP + pagina D-pad

  USO:
   1) Conectar el celu a la red  "Rover_IA"  (clave: rover1234)
   2) Abrir en el navegador:  http://192.168.4.1
   3) Mantener presionados los botones para mover.

  CONFIG PLACA: ESP32-S3 · USB CDC On Boot DISABLED · PSRAM OPI
                · Flash 16MB · upload por USB-C derecho (UART).
  ============================================================
*/
#include <Arduino.h>
#include "config.h"
#include "motor.h"
#include "web_server.h"

void setup(){
  Serial.begin(115200);
  delay(300);

  motorsBegin();      // pines, LEDC, coast inicial
  webServerBegin();   // Access Point + servidor HTTP
}

void loop(){
  webServerLoop();    // atiende la pagina de control
  motorsUpdate();     // watchdog de seguridad: frena si se cae el WiFi
}
