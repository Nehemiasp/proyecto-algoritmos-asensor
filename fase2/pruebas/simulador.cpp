// Arduino simulado: ejecuta ascensor.ino en la PC para probar la logica.
// Uso: ./correr_pruebas.sh (ver README de esta carpeta)
#include "Arduino.h"
#include "LiquidCrystal_I2C.h"
#include <map>
#include <vector>
#include <functional>
#include <cstdlib>
#include <cstring>

/* ---- mundo fisico simulado ---- */
unsigned long long T = 0;            // microsegundos
long posFis = 0;                     // pasos reales de la cabina sobre el piso 1
int pins[80]; int lastPhase = -1; long motorSteps = 0;
bool limiteAbajoConectado = true;
std::map<int,unsigned long long> pressUntil;
std::vector<std::string> logLines; std::string cur;
const long PP = 6827;
static const int SECT[8][4] = {{1,0,0,0},{1,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,1,1},{0,0,0,1},{1,0,0,1}};
Printer Serial;
void Printer::out(const std::string &s) {
  if (!serial) { ((LiquidCrystal_I2C*)this)->put(s); return; }
  for (char ch : s) { if (ch == '\n') { char b[32]; snprintf(b, 32, "[%7.2f s] ", T / 1e6); logLines.push_back(b + cur); cur.clear(); } else cur += ch; }
}
void pinMode(int, int) {}
void digitalWrite(int p, int v) {
  pins[p] = v;
  if (p == 11) {                      // se escribio la fase completa
    int ph = -1; for (int i = 0; i < 8; i++) if (SECT[i][0]==pins[8]&&SECT[i][1]==pins[9]&&SECT[i][2]==pins[10]&&SECT[i][3]==pins[11]) ph = i;
    if (ph >= 0 && lastPhase >= 0) { int d = (ph - lastPhase + 8) % 8; if (d == 1) { posFis++; motorSteps++; } else if (d == 7) { posFis--; motorSteps++; } }
    if (ph >= 0) lastPhase = ph;
  }
}
int digitalRead(int p) {
  if (p == 35) return (limiteAbajoConectado && posFis <= 0) ? LOW : HIGH;
  if (p == 36) return (posFis >= 3 * PP) ? LOW : HIGH;
  auto it = pressUntil.find(p); if (it != pressUntil.end() && T < it->second) return LOW;
  return HIGH;
}
unsigned long millis() { return (unsigned long)(T / 1000); }
unsigned long micros() { return (unsigned long)T; }
void delayMicroseconds(unsigned int us) { T += us; }

#include "sketch.cpp"

/* ---- utilidades de escenario ---- */
void press(int pin, double ms = 120) { pressUntil[pin] = T + (unsigned long long)(ms * 1000); }
void runFor(double s) { unsigned long long end = T + (unsigned long long)(s * 1e6); while (T < end) { loop(); T += 20; } }
bool idle() { if (estado != QUIETO || cambioModoPendiente) return false; for (int i = 1; i <= 4; i++) if (cabina[i]||subir[i]||bajar[i]) return false; return true; }
double runUntilIdle(double maxS = 200) { unsigned long long t0 = T; runFor(0.3); while (!idle() && (T - t0) < maxS * 1e6) runFor(0.05); return (T - t0) / 1e6; }
std::vector<int> stops() { std::vector<int> v; for (auto &l : logLines) { auto k = l.find("] Piso "); if (k != std::string::npos) v.push_back(atoi(l.c_str() + k + 7)); } return v; }
void dumpLog(size_t from) { for (size_t i = from; i < logLines.size(); i++) printf("    %s\n", logLines[i].c_str()); }
void lcdShow() { printf("    LCD: |%s|\n         |%s|\n", lcd.rows[0].c_str(), lcd.rows[1].c_str()); }
void posCheck() { printf("    Posicion: el programa cree piso %d; la cabina real esta a %ld pasos del piso 1 (%.2f pisos). %s\n", pisoActual, posFis, posFis / (double)PP, (labs(posFis - (pisoActual - 1) * PP) < 50) ? "COINCIDEN" : "DESFASADOS"); }

int main(int argc, char **argv) {
  std::string sc = argc > 1 ? argv[1] : "";
  posFis = (long)(1.4 * PP);           // arranca entre el piso 2 y el 3
  if (sc == "homingfail") limiteAbajoConectado = false;
  size_t mark = 0;
  // setup() bloquea en irAlOrigen; como el final de carrera abajo no responde en "homingfail", while(true) se cuelga: lo evitamos
  if (sc == "homingfail") {
    long limite = PASOS_PISO * (PISOS + 1); unsigned long long t0 = T;
    bool ok = irAlOrigen(); (void)limite;
    printf("  irAlOrigen() devolvio %s tras %.1f s y %ld pasos (limite: 5 pisos = %ld)\n", ok ? "true" : "false", (T - t0) / 1e6, motorSteps, PASOS_PISO * (PISOS + 1));
    return 0;
  }
  setup();
  printf("  Calibracion: %.1f s. ", T / 1e6); posCheck(); mark = logLines.size();
  if (sc == "demo") {
    for (int modo = 0; modo < 2; modo++) {
      if (modo == 0) { press(34); runFor(0.3); }                 // LOOK -> FCFS
      else { press(30); runUntilIdle(); runFor(3); press(34); runFor(0.3); }  // vuelve al 1, cambia a LOOK
      mark = logLines.size(); recorridos = 0;
      press(27); runFor(0.2); press(23); runFor(0.2); press(25);
      double t = runUntilIdle();
      printf("  Modo %s: %d pisos recorridos, %.1f s hasta terminar. Paradas/llegadas:", modoLOOK ? "LOOK" : "FCFS", recorridos, t);
      std::vector<int> st; for (size_t i = mark; i < logLines.size(); i++) { auto k = logLines[i].find("] Piso "); if (k != std::string::npos) st.push_back(atoi(logLines[i].c_str() + k + 7)); }
      printf(" 1"); for (int p : st) printf(" -> %d", p); printf("\n"); lcdShow(); dumpLog(mark);
    }
  }
  if (sc == "actual") {
    long s0 = motorSteps; press(30); runFor(0.2); bool abrio = false;
    for (int i = 0; i < 300; i++) { runFor(0.01); if (estado == PUERTA) abrio = true; }
    runUntilIdle();
    printf("  Cabina quieta en piso 1, apretan el boton 1: puerta %s, pasos del motor %ld, recorridos %d\n", abrio ? "abierta" : "NO abrio", motorSteps - s0, recorridos);
  }
  if (sc == "repetido") {
    press(23, 60); runFor(0.15); press(23, 60); runFor(0.15); press(23, 60); runFor(0.15); press(23, 60); runFor(0.15); press(23, 60);
    double t = runUntilIdle(); printf("  5 pulsaciones de piso 2 subir: recorridos=%d, tiempo %.1f s\n", recorridos, t); dumpLog(mark);
  }
  if (sc == "opuestos") {
    press(33); runFor(0.2); press(23); press(24); double t = runUntilIdle();
    printf("  Cabina 4 + piso 2 subir y bajar a la vez: recorridos=%d, %.1f s\n", recorridos, t); dumpLog(mark);
  }
  if (sc == "modo") {
    press(33); runFor(3.0);                 // va subiendo hacia el 4; a los 3 s esta entre piso 1 y 2
    printf("  Se aprieta MODO con la cabina a %.2f pisos del 1 (en movimiento)\n", posFis / (double)PP);
    press(34); runUntilIdle(); runFor(1); dumpLog(mark); posCheck(); lcdShow();
  }
  if (sc == "resbalo") {
    press(32); runUntilIdle(); printf("  En piso 3. Simulamos que la cabina perdio 500 pasos (resbalo).\n"); posFis -= 500; posCheck();
    mark = logLines.size(); press(30); runUntilIdle(); dumpLog(mark); posCheck();
  }
  if (sc == "puerta2") {
    press(32); runFor(0.2);
    while (!(estado == PUERTA && pisoActual == 3)) runFor(0.01);
    unsigned long abre = millis(); long s0 = motorSteps; int r0 = recorridos; mark = logLines.size();
    runFor(1.5); press(32); runFor(0.2);
    while (estado == PUERTA) runFor(0.01);
    unsigned long cierra = millis();
    runFor(1.0);
    printf("  Puerta abierta en piso 3; a los 1.5 s apretan el boton 3 otra vez.\n  La puerta estuvo abierta %.2f s (normal: 2.5 s). Pasos del motor: %ld. Pisos agregados: %d\n", (cierra - abre) / 1000.0, motorSteps - s0, recorridos - r0);
    dumpLog(mark);
  }
  return 0;
}
