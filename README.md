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

## Fase 2 — Prototipo 2.0 (versión beta)

En la segunda fase el proyecto pasó de la simulación en Wokwi a un prototipo listo para montar en la maqueta física.

- **Código versión 2.0** (`fase2/ascensor/ascensor.ino`), pensado para la maqueta real y compilado para el Arduino Mega 2560:
  - Dos finales de carrera (pines 35 y 36). Si la cabina toca uno, la posición se recalibra sola aunque se hayan perdido pasos.
  - Si el final de carrera del piso 1 no responde al encender, el programa se detiene y muestra `ERROR sensor P1` en lugar de forzar el motor.
  - El cambio de modo con la cabina en movimiento ahora se aplica cuando se detiene. En la versión anterior, la posición quedaba desfasada 827 pasos (más de 1 cm).
  - Si la puerta está abierta y alguien pide ese mismo piso, solo vuelve a contar el tiempo de la puerta.
  - La segunda línea de la pantalla cabe en 16 caracteres (`Rec:06 PUERTA P2`) y la flecha se borra cuando la cabina se queda sin pedidos.
  - Los textos fijos van en memoria flash con `F()`: la RAM usada bajó de 17.0 % a 13.5 %.
- **Pruebas sobre el código real** (`fase2/pruebas/`): un Arduino simulado en la computadora (reloj, pines, motor, finales de carrera y pantalla) ejecuta el mismo `ascensor.ino`. Se corrieron 9 casos y todos dieron el resultado esperado.
- **Modelos 3D** (se abren con doble clic en el navegador):
  - `Ascensor de 4 pisos · modelo 3D.html`: simulación del funcionamiento con FCFS y LOOK.
  - `Armado del ascensor.html`: guía de armado en 15 pasos, con la plantilla de cortes.
  - `Ascensor de 4 pisos · cableado y diagramas.html`: las 21 conexiones una por una, con su pin.
- **Estructura base** de la maqueta armada en cartón pluma de 5 mm, todavía sin componentes.

### Resultados

| Medición | FCFS | LOOK |
|---|---|---|
| Pisos recorridos en la demostración (desde el piso 1: 4 bajar, 2 subir, 3 subir) | 6 | 3 |
| Recorrido | 1 → 4 → 2 → 3 | 1 → 2 → 3 → 4 |
| Tiempo total, con 2.5 s de puerta en cada parada | 48.1 s | 27.6 s |

| Memoria del Arduino Mega 2560 | Versión 1 | Versión 2.0 |
|---|---|---|
| Flash (programa) | 10 354 bytes (3.9 %) | 10 692 bytes (4.1 %) |
| SRAM (variables) | 1 389 bytes (17.0 %) | 1 104 bytes (13.5 %) |

### Pendiente para la entrega final

1. Comprar y montar la electrónica y la mecánica en la estructura base.
2. Probar la dirección del motor antes de enganchar la cabina a la faja.
3. Calibrar `PASOS_PISO` midiendo la subida real de un piso.
4. Repetir los 9 casos de prueba en la maqueta y medir los tiempos con cronómetro.
5. Validar con usuarios y grabar el video de respaldo para la defensa.

## Archivos

| Archivo | Descripción |
|---|---|
| `ascensor.ino` | Firmware de la Fase 1, con `MODO_SIMULACION` para Wokwi. |
| `diagram.json` | Diagrama de conexiones para simular en [Wokwi](https://wokwi.com). |
| `fase2/ascensor/ascensor.ino` | Firmware versión 2.0 para la maqueta física. |
| `fase2/pruebas/` | Arduino simulado en la PC y los 9 casos de prueba, con sus resultados. |
| `Ascensor de 4 pisos · modelo 3D.html` | Simulación 3D del funcionamiento. |
| `Armado del ascensor.html` | Guía 3D de armado paso a paso. |
| `Ascensor de 4 pisos · cableado y diagramas.html` | Guía 3D de cableado y diagramas. |
| `docs/plantilla-cortes.png` | Plantilla para cortar las piezas en dos pliegos de 70 × 50 cm. |

## Hardware

- Arduino Mega 2560
- Motor paso a paso 28BYJ-48 + driver ULN2003 (pines 8–11)
- Pantalla LCD 16x2 con módulo I2C (dirección `0x27`, alternativa `0x3F`)
- 10 pulsadores (6 pasillo + 4 cabina) y 1 botón de modo
- Final de carrera para el homing (pin 35) y, desde la Fase 2, otro para el piso 4 (pin 36)
- Fuente de 5 V 2 A solo para el motor; el Arduino va por USB o con un adaptador de 9 V
- Faja GT2 de 852 mm con dos poleas GT2 de 30 dientes; pisos separados 10 cm

## Cómo probarlo

1. Abre el proyecto en el [simulador de Wokwi](https://wokwi.com) cargando `diagram.json`, o súbelo a un Arduino Mega real.
2. Deja `MODO_SIMULACION = true` para pruebas rápidas; cámbialo a `false` para la maqueta física.
3. Usa el botón de modo (pin 34) para alternar entre LOOK y FCFS y comparar los pisos recorridos.
4. Para la maqueta física de la Fase 2, sube `fase2/ascensor/ascensor.ino`. Requiere la librería LiquidCrystal I2C de Frank de Brabander.
5. Para repetir las pruebas de la Fase 2 en una computadora con Python 3 y g++: `sh fase2/pruebas/correr_pruebas.sh`.
