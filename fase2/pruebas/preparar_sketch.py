"""Convierte ascensor.ino en sketch.cpp agregando los prototipos de las
funciones, como hace el Arduino IDE antes de compilar."""
import re, sys
src = sys.argv[1] if len(sys.argv) > 1 else '../ascensor/ascensor.ino'
s = open(src, encoding='utf-8').read()
protos = re.findall(r'^((?:void|bool|int|long|byte)\s+\w+\([^)]*\))\s*\{', s, re.M)
i = s.index('LiquidCrystal_I2C lcd(')
protos = [p + ';' for p in protos if not p.startswith(('void setup', 'void loop'))]
open('sketch.cpp', 'w', encoding='utf-8').write(s[:i] + '\n'.join(protos) + '\n' + s[i:])
