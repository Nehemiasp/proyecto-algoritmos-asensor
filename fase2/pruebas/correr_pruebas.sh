#!/bin/sh
# Compila ascensor.ino con el Arduino simulado y corre los 9 casos de prueba.
set -e
cd "$(dirname "$0")"
python3 preparar_sketch.py ../ascensor/ascensor.ino
g++ -O2 -std=c++17 -w -Imock -I. simulador.cpp -o simulador
for caso in demo repetido opuestos modo resbalo actual puerta2 homingfail; do
  echo "=== $caso"
  ./simulador "$caso"
done
