/*
  ============================================================
  ROVER 4WD - Control WiFi (AP) por pagina web   (v1 / TB6612FNG)
  ------------------------------------------------------------
  Plataforma : ESP32-S3 N16R8
  Drivers    : 2x TB6612FNG  (un canal por motor = 4 indep.)
  Arduino    : ESP32 Core 3.x  (API LEDC nueva)
  ------------------------------------------------------------
  - El ESP32 crea su propia red WiFi (modo AP).
  - Sirve una pagina con D-pad: mantener presionado mueve, soltar frena.
  - Slider de velocidad (lazo abierto, sin PID todavia).
  - Watchdog: si no llegan comandos (WiFi caido), frena.

  USO:
   1) Conectar el celu a la red  "Rover_IA"  (clave: rover1234)
   2) Abrir en el navegador:  http://192.168.4.1
   3) Mantener presionados los botones para mover.

  CONFIG IDE: Placa "ESP32S3 Dev Module" · USB CDC On Boot DISABLED
              · PSRAM OPI · Flash 16MB · upload por USB-C derecho.
  ============================================================
*/

#include <WiFi.h>
#include <WebServer.h>

// ===================== RED WIFI (AP) ========================
const char* AP_SSID = "Rover_IA";
const char* AP_PASS = "rover1234";   // minimo 8 caracteres (cambiala si querés)
WebServer server(80);

// ===================== MAPA DE MOTORES ======================
//  0 = Front-Left (Mod L / A)   1 = Rear-Left (Mod L / B)
//  2 = Front-Right (Mod R / A)  3 = Rear-Right (Mod R / B)
#define N_MOTORS 4
const int PWM_PIN[N_MOTORS] = { 4, 7, 17, 47 };
const int IN1_PIN[N_MOTORS] = { 5, 15, 18, 40 };
const int IN2_PIN[N_MOTORS] = { 6, 16, 21, 41 };
const int STBY_PIN = 39;

// Inversion de sentido por motor (izquierdos invertidos, confirmado en banco)
bool motorInvert[N_MOTORS] = { true, true, false, false };

// ===================== CONFIG PWM ===========================
const int PWM_FREQ = 20000;
const int PWM_RES  = 8;
const int PWM_MAX  = 255;

// ===================== ESTADO ===============================
int currentSpeed = 140;                  // duty actual (0..255), arranca bajo
unsigned long lastCmdMs = 0;
const unsigned long WATCHDOG_MS = 800;   // corta si no hay comandos (WiFi caido)
bool motorsActive = false;

// ===================== CONTROL DE MOTOR =====================
void motorWrite(int m, int speed){
  bool fwd = (speed >= 0);
  if(motorInvert[m]) fwd = !fwd;
  int duty = abs(speed);
  if(duty > PWM_MAX) duty = PWM_MAX;
  if(speed == 0){
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
  for(int m=0;m<N_MOTORS;m++) motorWrite(m, 0);
  motorsActive = false;
}

// drive(left, right): skid steering
void drive(int left, int right){
  motorWrite(0, left);   motorWrite(1, left);
  motorWrite(2, right);  motorWrite(3, right);
  motorsActive = (left != 0 || right != 0);
  lastCmdMs = millis();
}

// ===================== PAGINA WEB ===========================
const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="es"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Rover IA</title>
<style>
:root{color-scheme:dark}
*{box-sizing:border-box;-webkit-user-select:none;user-select:none;-webkit-tap-highlight-color:transparent}
body{margin:0;font-family:system-ui,sans-serif;background:#0f1115;color:#e8eaed;
 display:flex;flex-direction:column;align-items:center;min-height:100vh;padding:18px}
h1{font-size:20px;font-weight:600;margin:6px 0 2px}
.sub{font-size:12px;color:#8b92a0;margin-bottom:20px;text-align:center}
.speed{width:100%;max-width:340px;margin-bottom:22px}
.speed label{display:flex;justify-content:space-between;font-size:14px;margin-bottom:6px}
input[type=range]{width:100%;height:34px}
.pad{display:grid;grid-template-columns:repeat(3,94px);grid-template-rows:repeat(3,94px);gap:12px}
button{font-size:26px;font-weight:700;border:none;border-radius:18px;background:#1c212b;
 color:#e8eaed;touch-action:none;transition:background .08s}
button:active{background:#2b6cff}
.fwd{grid-column:2;grid-row:1}.left{grid-column:1;grid-row:2}
.stp{grid-column:2;grid-row:2;background:#3a1f24;color:#ff6b6b;font-size:20px}
.stp:active{background:#e23a3a;color:#fff}
.right{grid-column:3;grid-row:2}.back{grid-column:2;grid-row:3}
</style></head>
<body>
<h1>Rover IA</h1>
<div class="sub">Mantené presionado para mover &middot; soltá para frenar</div>
<div class="speed">
 <label>Velocidad <span id="sv">140</span></label>
 <input type="range" id="sp" min="70" max="230" value="140">
</div>
<div class="pad">
 <button class="fwd"  data-d="F">&#9650;</button>
 <button class="left" data-d="L">&#9664;</button>
 <button class="stp"  data-d="S">STOP</button>
 <button class="right"data-d="R">&#9654;</button>
 <button class="back" data-d="B">&#9660;</button>
</div>
<script>
let dir='S', speed=140;
const sp=document.getElementById('sp'), sv=document.getElementById('sv');
sp.oninput=()=>{speed=+sp.value; sv.textContent=speed;};
function send(d){fetch('/m?d='+d+'&s='+speed).catch(()=>{});}
function press(d){dir=d; send(d);}
function release(){dir='S'; send('S');}
document.querySelectorAll('.pad button').forEach(b=>{
 const d=b.dataset.d;
 if(d==='S'){ b.addEventListener('pointerdown',e=>{e.preventDefault();release();}); }
 else{
  b.addEventListener('pointerdown',e=>{e.preventDefault();press(d);});
  b.addEventListener('pointerup',release);
  b.addEventListener('pointerleave',release);
  b.addEventListener('pointercancel',release);
 }
});
setInterval(()=>{ if(dir!=='S') send(dir); },200);
</script>
</body></html>
)HTML";

// ===================== HANDLERS HTTP ========================
void handleRoot(){
  server.send_P(200, "text/html", PAGE);
}

void handleMove(){
  String d = server.arg("d");
  int s = server.arg("s").toInt();
  if(s < 0) s = 0;
  if(s > PWM_MAX) s = PWM_MAX;
  currentSpeed = s;

  if      (d == "F") drive( currentSpeed,  currentSpeed);
  else if (d == "B") drive(-currentSpeed, -currentSpeed);
  else if (d == "L") drive(-currentSpeed,  currentSpeed);
  else if (d == "R") drive( currentSpeed, -currentSpeed);
  else               coastAll();   // "S" o desconocido

  server.send(200, "text/plain", "ok");
}

// ===================== SETUP ================================
void setup(){
  Serial.begin(115200);
  delay(300);

  pinMode(STBY_PIN, OUTPUT);
  digitalWrite(STBY_PIN, HIGH);
  for(int m=0;m<N_MOTORS;m++){
    pinMode(IN1_PIN[m], OUTPUT);
    pinMode(IN2_PIN[m], OUTPUT);
    digitalWrite(IN1_PIN[m], LOW);
    digitalWrite(IN2_PIN[m], LOW);
    ledcAttach(PWM_PIN[m], PWM_FREQ, PWM_RES);   // API Core 3.x
    ledcWrite(PWM_PIN[m], 0);
  }
  coastAll();

  // Levantar el Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();

  Serial.println();
  Serial.println(F("===== ROVER WiFi AP ====="));
  Serial.print  (F("Red:   ")); Serial.println(AP_SSID);
  Serial.print  (F("Clave: ")); Serial.println(AP_PASS);
  Serial.print  (F("Abrir: http://")); Serial.println(ip);
  Serial.println(F("========================="));

  server.on("/", handleRoot);
  server.on("/m", handleMove);
  server.onNotFound([](){ server.send(404, "text/plain", "404"); });
  server.begin();
}

// ===================== LOOP =================================
void loop(){
  server.handleClient();

  // Watchdog: si se cortó el WiFi y veníamos en movimiento, frenar
  if(motorsActive && (millis() - lastCmdMs > WATCHDOG_MS)){
    coastAll();
    Serial.println(F("[watchdog] sin comandos -> motores detenidos"));
  }
}
