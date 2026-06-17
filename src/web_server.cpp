#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "motor.h"
#include "web_server.h"

static WebServer server(80);
static int currentSpeed = DEFAULT_SPEED;   // ultimo duty pedido (0..255)

// ===================== PAGINA WEB ===========================
static const char PAGE[] PROGMEM = R"HTML(
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
static void handleRoot(){
  server.send_P(200, "text/html", PAGE);
}

static void handleMove(){
  String d = server.arg("d");
  int s = server.arg("s").toInt();
  if(s < 0)       s = 0;
  if(s > PWM_MAX) s = PWM_MAX;
  currentSpeed = s;

  if      (d == "F") drive( currentSpeed,  currentSpeed);
  else if (d == "B") drive(-currentSpeed, -currentSpeed);
  else if (d == "L") drive(-currentSpeed,  currentSpeed);
  else if (d == "R") drive( currentSpeed, -currentSpeed);
  else               coastAll();          // "S" o desconocido

  server.send(200, "text/plain", "ok");
}

// ===================== API DEL MODULO =======================
void webServerBegin(){
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

void webServerLoop(){
  server.handleClient();
}
