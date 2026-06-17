# Rover IA — Firmware del ESP32-S3

> Contexto persistente del proyecto para **Claude Code**. Este archivo se lee automáticamente al inicio de cada sesión. Mantenelo conciso y específico.
> Última actualización: 7 de junio de 2026.

---

## Qué es este proyecto

Rover terrestre **4WD con tracción diferencial** (skid steering), plataforma de experimentación en robótica autónoma. Arquitectura **dual processor**:

- **ESP32-S3** → control en tiempo real (motores, sensores, comunicación). **Este repo es el firmware del ESP32-S3.**
- **Jetson Orin NX 16GB** (Yahboom) → visión, IA y planificación. Fase 2, todavía no integrado.

La separación es deliberada: el ESP32 da tiempo determinístico para el control de bajo nivel; el Jetson aporta cómputo flexible para percepción. Se comunican por UART (Fase 2).

## Estado actual

**Fase 1 casi cerrada.** El rover ya se maneja por WiFi desde el celular: el ESP32 levanta su propia red, sirve una página con D-pad y mueve los 4 motores. Probado y funcional con fuente de banco alimentando los motores.

Lo que **falta** (y que Claude Code va a ayudar a construir):

1. **Modularizar** el firmware monolítico actual en una estructura PlatformIO con módulos `.cpp`/`.h`.
2. Cerrar la alimentación autónoma por batería (hardware, fuera de este repo).
3. **PID de velocidad por rueda** (cuando lleguen los encoders) y **PID de rumbo** con IMU.
4. **UART con el Jetson** (GPIO43/44).
5. Migrar el loop monolítico a **multi-task con FreeRTOS**.

---

## Cómo trabajar en este repo (entorno y build)

- Editor: **VS Code + Claude Code**.
- Toolchain: **PlatformIO**, pero con el fork de la comunidad **pioarduino**.
- **CRÍTICO:** el PlatformIO oficial (`platform = espressif32`) **NO soporta el Arduino Core 3.x**. Este firmware usa la API LEDC nueva (`ledcAttach` / `ledcWrite` con número de pin), que es exclusiva del Core 3.x. Por eso el `platformio.ini` apunta al platform de pioarduino. Si en algún momento el build falla con `ledcAttach was not declared`, casi seguro se cambió el platform a uno con Core 2.x.

### `platformio.ini`

```ini
[env:esp32-s3-devkitc-1]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
board_build.flash_size = 16MB
board_build.psram_type = opi
build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=0   ; equivale a "USB CDC On Boot = Disabled"
lib_deps =
    ; agregar acá las librerías con su versión, p. ej.:
    ; bblanchon/ArduinoJson@^7
```

### Comandos

```bash
pio run                  # compilar
pio run -t upload        # compilar y flashear
pio device monitor       # monitor serie (115200)
```

### Reglas de placa que NO se tocan

- `USB CDC On Boot = Disabled` (el flag `-DARDUINO_USB_CDC_ON_BOOT=0`). Si queda en Enabled, `Serial.print` rutea al USB y no se ve nada en el monitor cuando se programa por el puerto UART. Es la causa #1 de horas perdidas.
- Flashear por el puerto **USB-C derecho** (UART). El izquierdo es OTG, no sirve para programar.
- PWM por la **API LEDC de Core 3.x**: `ledcAttach(pin, freq, resolución)` + `ledcWrite(pin, valor)`. La API vieja (`ledcSetup` + `ledcAttachPin` + canal) NO compila.

---

## Hardware

| Componente | Detalle |
|---|---|
| MCU | ESP32-S3 N16R8 (DevKitC-1), doble núcleo LX7 @240 MHz, 16 MB flash, 8 MB PSRAM octal |
| Driver de motor | 2× TB6612FNG (MOSFET dual H-bridge), **un canal por motor** → 4 motores independientes |
| Motores | 4× TT DC (3–6 V) |
| Alimentación motores | VM = 7.4 V (pack 2S / fuente de banco) |
| Alimentación lógica | 3.3 V del ESP32 a VCC de ambos drivers |
| Masa | **GND común** entre ESP32, ambos drivers y la fuente — sin esto no anda nada |

Topología: **Módulo L** = ruedas izquierdas (Canal A → FL, Canal B → RL); **Módulo R** = ruedas derechas (Canal A → FR, Canal B → RR). `STBY` de ambos módulos unidos a un solo GPIO (enable/freno maestro).

### Pinout (canónico — debe coincidir con el firmware)

| Señal | GPIO | | Señal | GPIO |
|---|---|---|---|---|
| PWMA (FL) | 4 | | PWMA (FR) | 17 |
| AIN1 / AIN2 (FL) | 5 / 6 | | AIN1 / AIN2 (FR) | 18 / 21 |
| PWMB (RL) | 7 | | PWMB (RR) | 47 |
| BIN1 / BIN2 (RL) | 15 / 16 | | BIN1 / BIN2 (RR) | 40 / 41 |
| STBY (ambos) | 39 | | | |

### Pines reservados / prohibidos (no usar para nada nuevo sin avisar)

- **Reservados a futuro:** GPIO8/9 → I2C (MPU6050 `0x68` + OLED SSD1306 `0x3C`, Fase 2). GPIO43/44 → UART al Jetson (Fase 2). GPIO10–13 → encoder Hall en cuadratura del JGB37-520 (Fase 3).
- **Prohibidos por hardware:** GPIO35/36/37 (PSRAM octal), GPIO19/20 (USB OTG), GPIO0/45/46 (strapping de boot), GPIO38/48 (LEDs onboard).
- **Libres para expansión:** GPIO1, 2, 14, 42.

### Mapa de motores (índices del firmware)

| Índice | Posición | Módulo / Canal |
|---|---|---|
| 0 | Front-Left (FL) | L / A |
| 1 | Rear-Left (RL) | L / B |
| 2 | Front-Right (FR) | R / A |
| 3 | Rear-Right (RR) | R / B |

Arrays en el firmware:
`PWM_PIN = {4, 7, 17, 47}` · `IN1_PIN = {5, 15, 18, 40}` · `IN2_PIN = {6, 16, 21, 41}` · `STBY_PIN = 39`.

**Inversión de sentido:** `motorInvert = {true, true, false, false}` — los dos motores izquierdos van invertidos. Confirmado en banco. No cambiar sin re-verificar el sentido de giro físico.

---

## Arquitectura de software objetivo (lo que hay que construir)

El firmware actual es un `.ino` monolítico. El objetivo es refactorizarlo a esta estructura, **sin perder ninguna funcionalidad ni regla de seguridad** del firmware de referencia:

```
rover_firmware/
├── platformio.ini
├── CLAUDE.md                 # este archivo
├── src/
│   ├── main.cpp              # setup + dispatch; en Fase 2 arranca las tasks FreeRTOS
│   ├── config.h              # pines, constantes PWM, motorInvert, credenciales AP
│   ├── motor.h / motor.cpp   # abstracción TB6612FNG: motorWrite, drive, coastAll, brakeAll
│   ├── web_server.h/.cpp     # AP WiFi + servidor HTTP + página D-pad (PROGMEM)
│   └── (Fase 2) pid.cpp, imu.cpp, comm_uart.cpp, state.cpp
├── reference/
│   └── rover_wifi_ap_v1_tb6612.ino   # firmware funcional original (no compilar, es la fuente de verdad)
└── docs/
```

Orden sugerido de trabajo: **primero** separar `config.h` + `motor.*` y dejar el control de motores andando igual que el monolítico; **después** sacar el webserver a su módulo; **recién entonces** sumar features nuevas (PID, IMU, UART).

> **Nota (Windows):** el módulo del servidor se llama **`web_server.*`**, NO `webserver.*`. El filesystem de Windows es *case-insensitive*: un `webserver.h` propio en `src/` colisiona con el `<WebServer.h>` del framework Arduino y lo opaca (`'WebServer' does not name a type`), rompiendo el build. **No lo renombres de vuelta a `webserver`.**

---

## Lógica del firmware actual (fuente de verdad)

El sketch de referencia (`reference/rover_wifi_ap_v1_tb6612.ino`, incluido al final) funciona así:

**Red.** El ESP32 arranca en modo **Access Point**: levanta la red `Rover_IA` (clave `rover1234`) y queda en `http://192.168.4.1`. No se conecta a ningún router; el celular se conecta directo a él.

**Interfaz.** Sirve una única página HTML embebida en `PROGMEM` con un D-pad (▲ ◀ STOP ▶ ▼) y un slider de velocidad (rango 70–230). Es **control momentáneo**: `pointerdown` empieza a mover, `pointerup`/`pointerleave` frena. Mientras se mantiene presionado, el JS reenvía el comando cada **200 ms** (keepalive). Al soltar manda `S` (stop).

**Endpoints HTTP.** `GET /` devuelve la página. `GET /m?d=<F|B|L|R|S>&s=<0..255>` recibe dirección + velocidad y llama a `drive()` o `coastAll()`.

**Control de motores.** Tres capas:
- `motorWrite(m, speed)` — `speed` de −255 a 255. El signo es el sentido (corregido por `motorInvert[m]`); el valor absoluto es el duty del PWM. `speed = 0` → coast (rueda libre).
- `drive(left, right)` — skid steering: aplica `left` a los motores 0 y 1, `right` a los 2 y 3. Marca `motorsActive` y actualiza `lastCmdMs`.
- `coastAll()` / `brakeAll()` — frenan todo (coast = libre, brake = freno activo con IN1=IN2=HIGH).

**PWM.** `ledcAttach(pin, 20000, 8)` por cada pin de PWM en `setup()`. 20 kHz (inaudible), 8 bits (duty 0–255). Lazo abierto, sin PID todavía.

**Seguridad (watchdog).** En cada `loop()`, si los motores están activos y pasaron más de **800 ms** sin comandos (WiFi caído, pestaña cerrada), `coastAll()` frena solo. Como el navegador reenvía cada 200 ms, en operación normal nunca dispara; solo actúa ante un corte real. **Esta lógica de seguridad debe conservarse en todas las versiones.**

Hay además un segundo firmware de banco (`rover_test_serial_v3_1_tb6612.ino`, no incluido acá) que controla por monitor serie (`w/s/a/d`, `1`–`4` para testear motor por motor) y sirve para validar cableado y sentido de giro.

---

## Convenciones y reglas (imperativas)

- **Conservá siempre el watchdog de seguridad.** Cualquier modo de control nuevo debe frenar los motores ante pérdida de comunicación.
- **Mantené `motorInvert[]` como configuración**, no hardcodees el sentido en la lógica de `drive`.
- **No cambies la config crítica de placa** (`USB CDC On Boot = 0`, PSRAM OPI, flash 16 MB) ni el platform pioarduino.
- **Usá la API LEDC de Core 3.x.** Nunca `ledcSetup`/`ledcAttachPin`.
- **Centralizá pines y constantes en `config.h`.** Que el pinout viva en un solo lugar y coincida con la tabla de este archivo.
- **Respetá los pines reservados/prohibidos** de la tabla de hardware.
- **Comentarios y nombres de UI en español** (es el idioma del proyecto). Identificadores de código en inglés está bien.
- **Versioná los firmwares funcionales** con sufijo (`v1`, `v2`…) y dejá una copia congelada en `reference/` cuando una versión se aprueba en hardware.
- Antes de tocar el control de motores, **probá con las ruedas al aire**. Recordá que hay zona muerta: con carga real, por debajo de ~70–90 de duty puede que no arranquen.

---

## Roadmap / próximos pasos

**Fase 1 (cierre):** modularizar a PlatformIO · alimentación autónoma por batería (buck + pack 2S, hardware) · trim de PWM por rueda para compensar la asimetría izquierda/derecha (~36% medida).

**Fase 2 (visión + autonomía):**
- PID de velocidad por rueda (requiere encoders) y/o PID de rumbo con IMU **MPU6050** (I2C en GPIO8/9, dirección `0x68`).
- OLED **SSD1306** de telemetría (I2C, `0x3C`).
- **UART con el Jetson** (GPIO43/44, masa común, protocolo binario: STATE + CMD).
- Migrar el loop a **FreeRTOS multi-task** (control 50 Hz fijo, WiFi, UART, sensores en tasks separadas).

**Fase 3 (SLAM / mapeo):** encoders Hall en cuadratura con motores JGB37-520 (GPIO10–13, periférico PCNT) · visión / YOLO en el Jetson.

---

## Firmware de referencia (sketch funcional aprobado)

> Esta es la **fuente de verdad** de la lógica. Es un monolítico que compila y anda. Guardalo en `reference/` y úsalo de base para la modularización. No hace falta que compile desde acá.

```cpp
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
```
