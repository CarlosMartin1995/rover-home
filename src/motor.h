#pragma once

// ============================================================
//  motor.h - Abstraccion de los 4 motores sobre 2x TB6612FNG
//  ------------------------------------------------------------
//  Mapa de indices: 0=FL, 1=RL, 2=FR, 3=RR  (ver config.h).
//  El estado interno (activo / ultimo comando) vive en motor.cpp;
//  el watchdog de seguridad se atiende con motorsUpdate().
// ============================================================

// Configura pines, STBY y los canales LEDC. Llamar una vez en setup().
void motorsBegin();

// Escribe un motor. speed -255..255: el signo es el sentido (corregido por
// MOTOR_INVERT[]), el valor absoluto es el duty del PWM.
// speed = 0 -> coast (rueda libre).
void motorWrite(int m, int speed);

// Skid steering: aplica 'left' a los motores izquierdos (0,1) y 'right' a
// los derechos (2,3). Marca los motores como activos y refresca el watchdog.
void drive(int left, int right);

// Frenos.
void coastAll();   // rueda libre, sin par (IN1=IN2=LOW, duty 0)
void brakeAll();   // freno activo / short brake del TB6612 (IN1=IN2=HIGH)

// Watchdog de SEGURIDAD. Llamar en cada loop(): si veniamos en movimiento y
// se perdio la comunicacion (sin comandos por mas de WATCHDOG_MS), frena solo.
void motorsUpdate();
