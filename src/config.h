#pragma once

// ============================================================
//  config.h - Pinout, constantes y credenciales del Rover IA
//  ------------------------------------------------------------
//  Fuente UNICA de verdad del pinout. Debe coincidir con la
//  tabla de hardware del CLAUDE.md. No duplicar pines en otros
//  modulos: incluir este archivo.
// ============================================================

// ===================== RED WIFI (AP) ========================
constexpr char AP_SSID[] = "Rover_IA";
constexpr char AP_PASS[] = "rover1234";   // minimo 8 caracteres

// ===================== MAPA DE MOTORES ======================
//  0 = Front-Left  (Mod L / A)    1 = Rear-Left  (Mod L / B)
//  2 = Front-Right (Mod R / A)    3 = Rear-Right (Mod R / B)
constexpr int N_MOTORS = 4;
constexpr int PWM_PIN[N_MOTORS] = {  4,  7, 17, 47 };
constexpr int IN1_PIN[N_MOTORS] = {  5, 15, 18, 40 };
constexpr int IN2_PIN[N_MOTORS] = {  6, 16, 21, 41 };
constexpr int STBY_PIN = 39;              // enable/freno maestro (ambos drivers)

// Inversion de sentido por motor (izquierdos invertidos, confirmado en banco).
// Es CONFIGURACION: no hardcodear el sentido en la logica de drive().
constexpr bool MOTOR_INVERT[N_MOTORS] = { true, true, false, false };

// ===================== CONFIG PWM ===========================
constexpr int PWM_FREQ = 20000;           // 20 kHz, inaudible
constexpr int PWM_RES  = 8;               // 8 bits -> duty 0..255
constexpr int PWM_MAX  = 255;

// ===================== ESTADO / SEGURIDAD ===================
constexpr int           DEFAULT_SPEED = 140;   // duty inicial (0..255)
constexpr unsigned long WATCHDOG_MS   = 800;   // frena si no hay comandos (ms)
