#include <Arduino.h>
#include "config.h"
#include "motor.h"

// ===================== ESTADO INTERNO =======================
// Encapsulado en el modulo (no global): el watchdog lo consulta via
// motorsUpdate(); el resto del firmware no necesita verlo.
static bool          motorsActive = false;
static unsigned long lastCmdMs    = 0;

// ===================== INICIALIZACION =======================
void motorsBegin(){
  pinMode(STBY_PIN, OUTPUT);
  digitalWrite(STBY_PIN, HIGH);                  // habilita ambos TB6612

  for(int m = 0; m < N_MOTORS; m++){
    pinMode(IN1_PIN[m], OUTPUT);
    pinMode(IN2_PIN[m], OUTPUT);
    digitalWrite(IN1_PIN[m], LOW);
    digitalWrite(IN2_PIN[m], LOW);
    ledcAttach(PWM_PIN[m], PWM_FREQ, PWM_RES);   // API LEDC Core 3.x
    ledcWrite(PWM_PIN[m], 0);
  }
  coastAll();
}

// ===================== CONTROL DE MOTOR =====================
void motorWrite(int m, int speed){
  bool fwd = (speed >= 0);
  if(MOTOR_INVERT[m]) fwd = !fwd;

  int duty = abs(speed);
  if(duty > PWM_MAX) duty = PWM_MAX;

  if(speed == 0){                                // coast: rueda libre
    digitalWrite(IN1_PIN[m], LOW);
    digitalWrite(IN2_PIN[m], LOW);
    ledcWrite(PWM_PIN[m], 0);
    return;
  }
  if(fwd){ digitalWrite(IN1_PIN[m], HIGH); digitalWrite(IN2_PIN[m], LOW); }
  else   { digitalWrite(IN1_PIN[m], LOW);  digitalWrite(IN2_PIN[m], HIGH); }
  ledcWrite(PWM_PIN[m], duty);
}

void coastAll(){
  for(int m = 0; m < N_MOTORS; m++) motorWrite(m, 0);
  motorsActive = false;
}

void brakeAll(){
  for(int m = 0; m < N_MOTORS; m++){
    digitalWrite(IN1_PIN[m], HIGH);              // IN1=IN2=HIGH -> short brake
    digitalWrite(IN2_PIN[m], HIGH);
    ledcWrite(PWM_PIN[m], PWM_MAX);
  }
  motorsActive = false;
}

// drive(left, right): skid steering.
//   left  -> motores 0 (FL) y 1 (RL)
//   right -> motores 2 (FR) y 3 (RR)
void drive(int left, int right){
  motorWrite(0, left);   motorWrite(1, left);
  motorWrite(2, right);  motorWrite(3, right);
  motorsActive = (left != 0 || right != 0);
  lastCmdMs = millis();
}

// ===================== WATCHDOG ============================
void motorsUpdate(){
  if(motorsActive && (millis() - lastCmdMs > WATCHDOG_MS)){
    coastAll();
    Serial.println(F("[watchdog] sin comandos -> motores detenidos"));
  }
}
