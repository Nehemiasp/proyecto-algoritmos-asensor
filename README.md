# Ascensor de 4 Pisos — FCFS vs LOOK

Proyecto del curso de **Algoritmos (UMG)**. Simula un ascensor de 4 pisos sobre **Arduino Mega 2560** y compara dos algoritmos de planificación de solicitudes: **FCFS** (orden de llegada) y **LOOK** (planificación por sentido de movimiento).

## Fase 1 — Lógica de control y algoritmos

En esta primera fase se implementó el firmware completo del ascensor:

- **Lectura de botones** con antirrebote: 6 de pasillo (subir/bajar) + 4 de cabina + 1 de modo.
- **Dos algoritmos de planificación** intercambiables en caliente con el botón de modo (pin 34):
  - **FCFS**: atiende las solicitudes en el orden en que llegaron (cola FIFO).
  - **LOOK**: se mueve en un sentido atendiendo todo lo pendiente antes de invertir.
- **Control del motor paso a paso 28BYJ-48** con secuencia de medio paso.
- **Homing** al iniciar: baja hasta el final de carrera para calibrar el piso 1.
- **Pantalla LCD I2C 16x2**: muestra piso actual, dirección, modo, pisos recorridos y solicitudes pendientes.
- **Modo simulación** (`MODO_SIMULACION`) para probar en Wokwi con recorridos cortos.

## Archivos

| Archivo | Descripción |
|---|---|
| `ascensor.ino` | Firmware principal del ascensor. |
| `diagram.json` | Diagrama de conexiones para simular en [Wokwi](https://wokwi.com). |

## Hardware

- Arduino Mega 2560
- Motor paso a paso 28BYJ-48 + driver ULN2003 (pines 8–11)
- Pantalla LCD 16x2 con módulo I2C (dirección `0x27`, alternativa `0x3F`)
- 10 pulsadores (6 pasillo + 4 cabina) y 1 botón de modo
- Final de carrera para el homing (pin 35)

## Cómo probarlo

1. Abre el proyecto en el [simulador de Wokwi](https://wokwi.com) cargando `diagram.json`, o súbelo a un Arduino Mega real.
2. Deja `MODO_SIMULACION = true` para pruebas rápidas; cámbialo a `false` para la maqueta física.
3. Usa el botón de modo (pin 34) para alternar entre LOOK y FCFS y comparar los pisos recorridos.
