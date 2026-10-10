/*
  ASCENSOR DE 4 PISOS - Planificacion FCFS vs LOOK
  Curso de Algoritmos - UMG
  Placa: Arduino Mega 2560

  Traccion: motor 28BYJ-48 + faja GT2 con poleas de 30 dientes.
  Pisos separados 10 cm.

  Boton MODO (pin 34) alterna entre los dos algoritmos.
  El LCD muestra piso, direccion, modo y pisos recorridos.

  Version 2.0 (Fase 2):
    - El cambio de modo se aplica cuando la cabina esta detenida.
    - Si la puerta esta abierta y piden ese mismo piso, la puerta
      vuelve a contar su tiempo en lugar de registrar otra llamada.
    - Los textos fijos van en memoria flash con F() para ahorrar RAM.
    - La segunda linea del LCD cabe en 16 caracteres y la flecha
      se borra cuando la cabina queda sin trabajo.

  LO UNICO QUE PODES NECESITAR CAMBIAR:
    INVERTIR_MOTOR  -> si al arrancar la cabina SUBE en vez de bajar
    PASOS_PISO      -> si la cabina no sube exactamente 10 cm por piso
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);   // si la pantalla no muestra nada, proba 0x3F

// ---------- Ajustes ----------
const bool INVERTIR_MOTOR = false;     // cambiar a true si el motor gira al reves
const long PASOS_PISO     = 6827;      // 10 cm con polea GT2 de 30 dientes
const unsigned long US_PASO   = 1000;  // velocidad (no bajar de 1000)
const unsigned long MS_PUERTA = 2500;  // tiempo con la puerta "abierta"
const unsigned long MS_REBOTE = 25;    // antirrebote de botones

// ---------- Pines ----------
const byte MOTOR[4] = {8, 9, 10, 11};              // IN1..IN4 del driver

const byte BTN_PASILLO[6] = {22, 23, 24, 25, 26, 27};
const byte BTN_CABINA[4]  = {30, 31, 32, 33};
const byte PIN_MODO        = 34;
const byte PIN_LIMITE_BAJO = 35;   // final de carrera piso 1
const byte PIN_LIMITE_ALTO = 36;   // final de carrera piso 4

// Que representa cada boton de pasillo
//   22: P1 subir | 23: P2 subir | 24: P2 bajar
//   25: P3 subir | 26: P3 bajar | 27: P4 bajar
const byte PISO_PASILLO[6] = {1, 2, 2, 3, 3, 4};
const bool ES_SUBIR[6]     = {true, true, false, true, false, false};

const int PISOS = 4;

// Secuencia de medio paso del 28BYJ-48
const byte SEC[8][4] = {
  {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,1,0},
  {0,0,1,0}, {0,0,1,1}, {0,0,0,1}, {1,0,0,1}
};

// ---------- Estado ----------
int  pisoActual = 1;
int  dir        = 0;          // 1 sube, -1 baja, 0 quieto
long pasosRestantes = 0;
byte fase = 0;
unsigned long ultimoPaso = 0;

bool cabina[PISOS + 1];
bool subir[PISOS + 1];
bool bajar[PISOS + 1];

bool modoLOOK   = true;
bool cambioModoPendiente = false;   // se aplica al quedar QUIETO
int  recorridos = 0;

// Cola FIFO para FCFS
int  cola[20];
byte colaIni = 0, colaFin = 0;

enum Estado { QUIETO, MOVIENDO, PUERTA };
Estado estado = QUIETO;
unsigned long tPuerta = 0;

bool estadoPrevio[11];
unsigned long ultimaLectura = 0;

// ============================================================
void setup() {
  Serial.begin(9600);

  for (byte i = 0; i < 4; i++) pinMode(MOTOR[i], OUTPUT);
  for (byte i = 0; i < 6; i++) pinMode(BTN_PASILLO[i], INPUT_PULLUP);
  for (byte i = 0; i < 4; i++) pinMode(BTN_CABINA[i], INPUT_PULLUP);
  pinMode(PIN_MODO, INPUT_PULLUP);
  pinMode(PIN_LIMITE_BAJO, INPUT_PULLUP);
  pinMode(PIN_LIMITE_ALTO, INPUT_PULLUP);

  for (byte i = 0; i < 11; i++) estadoPrevio[i] = false;

  lcd.init();
  lcd.backlight();
  lcd.print(F("Calibrando..."));
  Serial.println(F("Calibrando: bajando hasta el piso 1..."));

  limpiarTodo();

  if (!irAlOrigen()) {
    lcd.clear();
    lcd.print(F("ERROR sensor P1"));
    lcd.setCursor(0, 1);
    lcd.print(F("Revisar cableado"));
    Serial.println(F("ERROR: el final de carrera del piso 1 no respondio."));
    while (true) { }               // se detiene aqui a proposito
  }

  Serial.println(F("Listo. Piso 1."));
  lcd.clear();
  actualizarLCD();
}

void loop() {
  leerBotones();

  switch (estado) {
    case QUIETO:
      if (cambioModoPendiente) aplicarCambioModo();
      decidir();
      break;
    case MOVIENDO: mover();   break;
    case PUERTA:
      if (millis() - tPuerta > MS_PUERTA) {
        estado = QUIETO;
        actualizarLCD();
      }
      break;
  }
}

// ============================================================
// Baja hasta tocar el final de carrera del piso 1.
// Si no lo encuentra tras recorrer 5 pisos, se rinde.
bool irAlOrigen() {
  long limite = PASOS_PISO * (PISOS + 1);
  long dados  = 0;

  while (digitalRead(PIN_LIMITE_BAJO) == HIGH && dados < limite) {
    motorPaso(-1);
    delayMicroseconds(US_PASO);
    dados++;
  }
  apagarMotor();

  if (dados >= limite) return false;

  pisoActual = 1;
  dir = 0;
  return true;
}

// ============================================================
// BOTONES
void leerBotones() {
  if (millis() - ultimaLectura < MS_REBOTE) return;
  ultimaLectura = millis();

  for (byte i = 0; i < 6; i++) {
    bool ahora = (digitalRead(BTN_PASILLO[i]) == LOW);
    if (ahora && !estadoPrevio[i]) registrarPasillo(i);
    estadoPrevio[i] = ahora;
  }

  for (byte i = 0; i < 4; i++) {
    bool ahora = (digitalRead(BTN_CABINA[i]) == LOW);
    if (ahora && !estadoPrevio[6 + i]) registrarCabina(i + 1);
    estadoPrevio[6 + i] = ahora;
  }

  bool m = (digitalRead(PIN_MODO) == LOW);
  if (m && !estadoPrevio[10]) {
    // Si la cabina va en camino, el cambio espera a que se detenga.
    // Cambiar a mitad de un piso dejaria la cuenta de pasos desfasada.
    cambioModoPendiente = true;
    Serial.println(F("Cambio de modo solicitado"));
  }
  estadoPrevio[10] = m;
}

void aplicarCambioModo() {
  cambioModoPendiente = false;
  modoLOOK = !modoLOOK;
  limpiarTodo();
  recorridos = 0;
  dir = 0;
  actualizarLCD();
  Serial.print(F("Modo: "));
  Serial.println(modoLOOK ? F("LOOK") : F("FCFS"));
}

// Puerta abierta en el piso p: si piden ese mismo piso, solo se
// reinicia el tiempo de la puerta (no se registra otra llamada).
bool alargarPuerta(int p) {
  if (estado != PUERTA || p != pisoActual) return false;
  tPuerta = millis();
  Serial.print(F("Puerta abierta de nuevo en piso "));
  Serial.println(p);
  return true;
}

void registrarPasillo(byte idx) {
  int p = PISO_PASILLO[idx];
  bool mismoSentido = (dir == 0) || (dir == 1 && ES_SUBIR[idx]) || (dir == -1 && !ES_SUBIR[idx]);
  if (mismoSentido && alargarPuerta(p)) return;
  if (ES_SUBIR[idx]) subir[p] = true;
  else               bajar[p] = true;
  encolar(p);
  actualizarLCD();
  Serial.print(F("Llamada piso "));
  Serial.print(p);
  Serial.println(ES_SUBIR[idx] ? F(" subir") : F(" bajar"));
}

void registrarCabina(int p) {
  if (alargarPuerta(p)) return;
  cabina[p] = true;
  encolar(p);
  actualizarLCD();
  Serial.print(F("Cabina destino "));
  Serial.println(p);
}

// ============================================================
// COLA FIFO (FCFS)
void encolar(int p) {
  for (byte i = colaIni; i != colaFin; i = (i + 1) % 20)
    if (cola[i] == p) return;
  cola[colaFin] = p;
  colaFin = (colaFin + 1) % 20;
}

bool colaVacia() { return colaIni == colaFin; }

void desencolar() {
  if (!colaVacia()) colaIni = (colaIni + 1) % 20;
}

void limpiarTodo() {
  for (int i = 0; i <= PISOS; i++) {
    cabina[i] = false;
    subir[i]  = false;
    bajar[i]  = false;
  }
  colaIni = colaFin = 0;
}

// ============================================================
// ALGORITMOS
void decidir() {
  int antes = dir;
  if (modoLOOK) decidirLOOK();
  else          decidirFCFS();
  if (dir == 0 && antes != 0) actualizarLCD();   // quita la flecha al quedar sin trabajo
}

void decidirLOOK() {
  if (debeParar(pisoActual, dir)) {
    limpiarPiso(pisoActual, dir);
    abrirPuerta();
    return;
  }

  if (dir == 1 && !hayArriba(pisoActual))
    dir = hayAbajo(pisoActual) ? -1 : 0;
  else if (dir == -1 && !hayAbajo(pisoActual))
    dir = hayArriba(pisoActual) ? 1 : 0;
  else if (dir == 0)
    dir = hayArriba(pisoActual) ? 1 : (hayAbajo(pisoActual) ? -1 : 0);

  if (dir != 0) iniciarMovimiento();
}

void decidirFCFS() {
  if (colaVacia()) { dir = 0; return; }

  int destino = cola[colaIni];

  if (destino == pisoActual) {
    cabina[destino] = false;
    subir[destino]  = false;
    bajar[destino]  = false;
    desencolar();
    abrirPuerta();
    return;
  }

  dir = (destino > pisoActual) ? 1 : -1;
  iniciarMovimiento();
}

// Regla central de LOOK
bool debeParar(int p, int d) {
  if (cabina[p]) return true;
  if (d >= 0 && subir[p]) return true;
  if (d <= 0 && bajar[p]) return true;
  if (d == 1  && bajar[p] && !hayArriba(p)) return true;   // punto de retorno
  if (d == -1 && subir[p] && !hayAbajo(p))  return true;
  return false;
}

void limpiarPiso(int p, int d) {
  cabina[p] = false;
  if (d >= 0) subir[p] = false;
  if (d <= 0) bajar[p] = false;
  if (d == 1  && !hayArriba(p)) bajar[p] = false;
  if (d == -1 && !hayAbajo(p))  subir[p] = false;
}

bool hayArriba(int p) {
  for (int i = p + 1; i <= PISOS; i++)
    if (cabina[i] || subir[i] || bajar[i]) return true;
  return false;
}

bool hayAbajo(int p) {
  for (int i = 1; i < p; i++)
    if (cabina[i] || subir[i] || bajar[i]) return true;
  return false;
}

// ============================================================
// MOVIMIENTO
void iniciarMovimiento() {
  pasosRestantes = PASOS_PISO;
  estado = MOVIENDO;
  actualizarLCD();
}

void mover() {
  if (micros() - ultimoPaso < US_PASO) return;
  ultimoPaso = micros();

  // Si toca un extremo, la posicion real manda sobre la cuenta.
  if (dir == 1 && digitalRead(PIN_LIMITE_ALTO) == LOW) {
    llegarAPiso(PISOS);
    return;
  }
  if (dir == -1 && digitalRead(PIN_LIMITE_BAJO) == LOW) {
    llegarAPiso(1);
    return;
  }

  motorPaso(dir);
  pasosRestantes--;

  if (pasosRestantes <= 0) llegarAPiso(pisoActual + dir);
}

void llegarAPiso(int p) {
  apagarMotor();
  pasosRestantes = 0;

  if (p != pisoActual) recorridos++;
  pisoActual = p;
  estado = QUIETO;

  actualizarLCD();
  Serial.print(F("Piso "));
  Serial.print(pisoActual);
  Serial.print(F(" | recorridos: "));
  Serial.println(recorridos);
}

void motorPaso(int d) {
  if (INVERTIR_MOTOR) d = -d;
  fase = (fase + (d > 0 ? 1 : 7)) % 8;
  for (byte i = 0; i < 4; i++)
    digitalWrite(MOTOR[i], SEC[fase][i]);
}

void apagarMotor() {
  for (byte i = 0; i < 4; i++) digitalWrite(MOTOR[i], LOW);
}

void abrirPuerta() {
  estado = PUERTA;
  tPuerta = millis();
  actualizarLCD();
}

// ============================================================
void actualizarLCD() {
  lcd.setCursor(0, 0);
  lcd.print(F("Piso "));
  lcd.print(pisoActual);
  lcd.print(dir == 1 ? F(" ^ ") : (dir == -1 ? F(" v ") : F("   ")));
  lcd.print(modoLOOK ? F(" LOOK ") : F(" FCFS "));

  int pendientes = 0;
  for (int i = 1; i <= PISOS; i++)
    if (cabina[i] || subir[i] || bajar[i]) pendientes++;

  lcd.setCursor(0, 1);
  // 16 caracteres exactos: "Rec:06 PUERTA P2" (P = pisos con pedidos pendientes)
  lcd.print(F("Rec:"));
  if (recorridos < 10) lcd.print(F("0"));
  lcd.print(recorridos);
  lcd.print(estado == PUERTA ? F(" PUERTA P") : F("        P"));
  lcd.print(pendientes);
}
