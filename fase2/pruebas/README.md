# Pruebas de la Fase 2

`simulador.cpp` ejecuta el mismo `fase2/ascensor/ascensor.ino` en una computadora, con versiones simuladas de:

- el reloj (`millis`, `micros`, `delayMicroseconds`),
- los pines de entrada y salida,
- el motor: la cabina avanza un paso cada vez que el código cambia la secuencia de las bobinas,
- los finales de carrera de los pisos 1 y 4, que se activan según la posición de la cabina,
- la pantalla LCD y el monitor serie.

En cada caso la cabina arranca entre el piso 2 y el 3, y el programa la calibra bajando al piso 1.

## Cómo correrlas

Necesitas Python 3 y g++:

```sh
sh correr_pruebas.sh
```

`resultados.txt` tiene la salida de la última corrida.

## Casos

| Caso | Qué prueba |
|---|---|
| `demo` | Demostración en FCFS y en LOOK: 4 bajar, 2 subir, 3 subir desde el piso 1 |
| `repetido` | 5 pulsaciones seguidas del mismo botón cuentan como una sola llamada |
| `opuestos` | Subir y bajar a la vez en el piso 2, con destino 4 en la cabina |
| `modo` | Botón MODO con la cabina en movimiento |
| `resbalo` | La cabina pierde 500 pasos y el final de carrera corrige la posición |
| `actual` | Llamada al piso donde ya está la cabina |
| `puerta2` | Llamada al mismo piso con la puerta abierta |
| `homingfail` | El final de carrera del piso 1 no responde al encender |
