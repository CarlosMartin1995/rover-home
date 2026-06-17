# Arquitectura del firmware (Fase 1 — modularizado)

Este documento describe la estructura del firmware tras el bootstrap a
PlatformIO. La **fuente de verdad** de la lógica sigue siendo el sketch
monolítico congelado en [`reference/rover_wifi_ap_v1_tb6612.ino`](../reference/rover_wifi_ap_v1_tb6612.ino).
La versión modular **no agrega ni quita funcionalidad** respecto de ese
sketch: solo lo separa en módulos. Ver el [CLAUDE.md](../CLAUDE.md) para el
contexto completo del proyecto.

## Mapa de módulos

| Archivo | Responsabilidad |
|---|---|
| `src/main.cpp` | `setup()` + `loop()`. Solo orquesta: inicializa los módulos y, en cada vuelta, atiende el webserver y corre el watchdog. |
| `src/config.h` | **Fuente única** de pinout, constantes PWM, `MOTOR_INVERT[]`, credenciales del AP y tiempos de seguridad. Debe coincidir con la tabla de hardware del CLAUDE.md. |
| `src/motor.h/.cpp` | Abstracción de los 4 motores sobre 2× TB6612FNG: `motorWrite`, `drive` (skid steering), `coastAll`, `brakeAll`, y el **watchdog** (`motorsUpdate`). El estado (`motorsActive`, `lastCmdMs`) queda encapsulado. |
| `src/webserver.h/.cpp` | Access Point WiFi + servidor HTTP + página D-pad (PROGMEM). Traduce los endpoints a llamadas del módulo `motor`. |

## Flujo

```
main::setup()  -> motorsBegin()      (pines, LEDC, coast inicial)
               -> webServerBegin()   (AP "Rover_IA", rutas HTTP)

main::loop()   -> webServerLoop()    (server.handleClient)
               -> motorsUpdate()     (watchdog de seguridad)
```

## Endpoints HTTP

- `GET /` → página de control (D-pad + slider de velocidad 70–230).
- `GET /m?d=<F|B|L|R|S>&s=<0..255>` → dirección + velocidad. Llama a `drive()`
  o `coastAll()`. El navegador reenvía el comando cada 200 ms (keepalive);
  al soltar manda `S`.

## Regla de seguridad (no negociable)

El **watchdog** vive en `motor.cpp` (`motorsUpdate()`, llamado en cada `loop()`):
si los motores están activos y pasan más de `WATCHDOG_MS` (800 ms) sin un nuevo
comando —WiFi caído, pestaña cerrada— frena solo con `coastAll()`. En operación
normal nunca dispara porque el keepalive llega cada 200 ms. **Cualquier modo de
control nuevo debe conservar esta protección.**

## Mapeo monolítico → modular

| En el monolítico | En la versión modular |
|---|---|
| globals de pinout/PWM | `config.h` (como `constexpr`) |
| `motorWrite` / `drive` / `coastAll` | `motor.cpp` (+ `brakeAll`, ya descrito en CLAUDE.md) |
| `motorsActive` / `lastCmdMs` | estado `static` en `motor.cpp` |
| watchdog en `loop()` | `motorsUpdate()` en `motor.cpp` |
| `PAGE`, `handleRoot`, `handleMove`, AP setup | `webserver.cpp` |
| `currentSpeed` | `static` en `webserver.cpp` |

## Próximo (Fase 2)

Se sumarán como módulos nuevos sin tocar los anteriores: `pid.cpp`, `imu.cpp`,
`comm_uart.cpp`, `state.cpp`, y la migración del `loop` a tasks FreeRTOS.
