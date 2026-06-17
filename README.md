# rover-home

Firmware del **ESP32-S3** del Rover IA (4WD skid steering), en **PlatformIO**
con el fork de la comunidad **pioarduino** (necesario para el Arduino Core 3.x).

> El contexto completo del proyecto (hardware, pinout, reglas y roadmap) está en
> [CLAUDE.md](CLAUDE.md). La lógica de referencia, en
> [reference/rover_wifi_ap_v1_tb6612.ino](reference/rover_wifi_ap_v1_tb6612.ino).

## Qué hace

El ESP32 levanta su propia red WiFi (`Rover_IA` / `rover1234`), sirve una página
con D-pad en `http://192.168.4.1` y mueve los 4 motores (2× TB6612FNG). Control
momentáneo con watchdog de seguridad.

## Estructura

```
rover-home/
├── platformio.ini            # entorno esp32-s3-devkitc-1 (pioarduino)
├── src/
│   ├── main.cpp              # setup + loop (orquesta los módulos)
│   ├── config.h              # pinout, PWM, MOTOR_INVERT, credenciales AP
│   ├── motor.h / motor.cpp   # TB6612FNG: drive, coast/brake, watchdog
│   └── web_server.h/.cpp     # AP WiFi + HTTP + página D-pad
├── reference/                # firmware monolítico congelado (fuente de verdad)
└── docs/                     # arquitectura.md
```

## Build (con la extensión PlatformIO de VS Code)

```bash
pio run                  # compilar
pio run -t upload        # compilar y flashear (USB-C derecho = UART)
pio device monitor       # monitor serie (115200)
```

> **Recordá:** `USB CDC On Boot = Disabled`, PSRAM OPI, Flash 16 MB, y el
> platform **pioarduino** (no el `espressif32` oficial). Detalles en CLAUDE.md.

## Uso

1. Conectar el celular a la red **`Rover_IA`** (clave `rover1234`).
2. Abrir **`http://192.168.4.1`**.
3. Mantener presionados los botones del D-pad para mover; soltar frena.
