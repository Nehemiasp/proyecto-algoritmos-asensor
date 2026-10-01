/*
  Ascensor de 4 pisos - Planificacion FCFS vs LOOK
  Curso de Algoritmos - UMG
  Placa: Arduino Mega 2560

  Boton de modo (pin 34) alterna entre los dos algoritmos.
  El LCD muestra piso actual, direccion, modo, pisos recorridos
  y solicitudes pendientes.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// true  = pruebas en Wokwi (pisos cortos para no esperar tanto)
// false = maqueta real
const bool MODO_SIMULACION = true;

LiquidCrystal_I2C lcd(0x27, 16, 2);   // si no enciende, prueba 0x3F

// ---------- Pines ----------
const byte MOTOR[4]       = {8, 9, 10, 11};
const byte BTN_PASILLO[6] = {22, 23, 24, 25, 26, 27};
const byte BTN_CABINA[4]  = {30, 31, 32, 33};
const byte PIN_MODO       = 34;
const byte PIN_LIMITE     = 35;

// Que solicitud representa cada boton de pasillo
const byte PISO_PASILLO[6] = {1, 2, 2, 3, 3, 4};
const bool ES_SUBIR[6]     = {true, true, false, true, false, false};

// ---------- Constantes a calibrar ----------
const int  PISOS = 4;
const long PASOS_PISO = MODO_SIMULACION ? 1500 : 6500;  // ajustar segun tu polea
const unsigned long US_PASO   = 1400;   // mas alto = mas lento
const unsigned long MS_PUERTA = 2500;   // tiempo de puerta abierta
const unsigned long MS_REBOTE = 25;     // antirrebote
const byte COLA_MAX = 20;

// Secuencia de medio paso del 28BYJ-48
const byte SEC[8][4] = {
  {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,1,0},
  {0,0,1,0}, {0,0,1,1}, {0,0,0,1}, {1,0,0,1}
};

// ---------- Estado del ascensor ----------
int  pisoActual = 1;
int  dir = 0;                 // 1 sube, -1 baja, 0 quieto
long pasosRestantes = 0;
byte fase = 0;
unsigned long ultimoPaso = 0;

bool cabina[PISOS + 1];
bool subir[PISOS + 1];
bool bajar[PISOS + 1];

bool modoLOOK = true;
int  recorridos = 0;

// Cola FIFO para el modo FCFS
int  cola[COLA_MAX];
byte colaIni = 0, colaFin = 0;

enum Estado { QUIETO, MOVIENDO, PUERTA };
Estado estado = QUIETO;
unsigned long tPuerta = 0;

// Antirrebote: 6 pasillo + 4 cabina + 1 modo
bool estadoPrevio[11];
unsigned long ultimaLectura = 0;

// Prototipos
void leerBotones();
void registrarPasillo(byte idx);
void registrarCabina(int p);
void encolar(int p);
bool colaVacia();
void desencolar();
void limpiarTodo();
void decidir();
void decidirLOOK();
void decidirFCFS();
bool debeParar(int p, int d);
void limpiarPiso(int p, int d);
bool hayArriba(int p);
bool hayAbajo(int p);
void iniciarMovimiento();
void mover();
void motorPaso(int d);
void apagarMotor();
void abrirPuerta();
void irAlOrigen();
void actualizarLCD();
void imprimirLinea(byte fila, const char* texto);

// ============================================================
void setup() {
  Serial.begin(9600);

  for (byte i = 0; i < 4; i++) pinMode(MOTOR[i], OUTPUT);
  for (byte i = 0; i < 6; i++) pinMode(BTN_PASILLO[i], INPUT_PULLUP);
  for (byte i = 0; i < 4; i++) pinMode(BTN_CABINA[i], INPUT_PULLUP);
  pinMode(PIN_MODO, INPUT_PULLUP);
  pinMode(PIN_LIMITE, INPUT_PULLUP);

  for (byte i = 0; i < 11; i++) estadoPrevio[i] = false;

  lcd.init();
  lcd.backlight();
  imprimirLinea(0, "Calibrando...");
  imprimirLinea(1, "");

  limpiarTodo();
  irAlOrigen();
  actualizarLCD();
  Serial.println("Listo. Modo: LOOK");
}

void loop() {
  leerBotones();

  switch (estado) {
    case QUIETO:   decidir(); break;
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
// Homing: baja hasta presionar el final de carrera
void irAlOrigen() {
  Serial.println("Homing: bajando hasta el final de carrera (pin 35)...");
  while (digitalRead(PIN_LIMITE) == HIGH) {
    motorPaso(-1);
    delayMicroseconds(US_PASO);
  }
  apagarMotor();
  pisoActual = 1;
  dir = 0;
  Serial.println("Final de carrera detectado: piso 1");
}

// ============================================================
// LECTURA DE BOTONES
// Se llama en cada vuelta del loop, incluso mientras el motor se
// mueve, para que las solicitudes se registren en cualquier momento.
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
    if (estado == MOVIENDO) {
      // Cambiar de modo entre pisos dejaria la cabina desubicada
      Serial.println("Espera a que llegue a un piso para cambiar de modo");
    } else {
      modoLOOK = !modoLOOK;
      limpiarTodo();
      recorridos = 0;
      dir = 0;
      actualizarLCD();
      Serial.print("Modo: ");
      Serial.println(modoLOOK ? "LOOK" : "FCFS");
    }
  }
  estadoPrevio[10] = m;
}

void registrarPasillo(byte idx) {
  int p = PISO_PASILLO[idx];
  if (ES_SUBIR[idx]) subir[p] = true;
  else               bajar[p] = true;
  encolar(p);
  actualizarLCD();
  Serial.print("Llamada piso ");
  Serial.print(p);
  Serial.println(ES_SUBIR[idx] ? " subir" : " bajar");
}

void registrarCabina(int p) {
  cabina[p] = true;
  encolar(p);
  actualizarLCD();
  Serial.print("Cabina destino ");
  Serial.println(p);
}

// ============================================================
// COLA FIFO (modo FCFS)
void encolar(int p) {
  for (byte i = colaIni; i != colaFin; i = (i + 1) % COLA_MAX)
    if (cola[i] == p) return;           // ya esta pendiente
  cola[colaFin] = p;
  colaFin = (colaFin + 1) % COLA_MAX;
}

bool colaVacia() { return colaIni == colaFin; }

void desencolar() {
  if (!colaVacia()) colaIni = (colaIni + 1) % COLA_MAX;
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
// EL ALGORITMO
void decidir() {
  if (modoLOOK) decidirLOOK();
  else          decidirFCFS();
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
  if (d == 1  && bajar[p] && !hayArriba(p)) return true;  // punto de retorno
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

  motorPaso(dir);
  pasosRestantes--;

  if (pasosRestantes <= 0) {
    apagarMotor();
    pisoActual += dir;
    recorridos++;
    estado = QUIETO;
    actualizarLCD();
    Serial.print("Piso ");
    Serial.print(pisoActual);
    Serial.print(" | recorridos: ");
    Serial.println(recorridos);
  }
}

void motorPaso(int d) {
  fase = (fase + (d > 0 ? 1 : 7)) % 8;
  for (byte i = 0; i < 4; i++) digitalWrite(MOTOR[i], SEC[fase][i]);
}

void apagarMotor() {
  for (byte i = 0; i < 4; i++) digitalWrite(MOTOR[i], LOW);
}

void abrirPuerta() {
  estado = PUERTA;
  tPuerta = millis();
  actualizarLCD();
  Serial.print("Puerta abierta en piso ");
  Serial.println(pisoActual);
}

// ============================================================
// LCD: cada linea se rellena a 16 caracteres para que no queden
// restos de textos anteriores en pantalla.
void imprimirLinea(byte fila, const char* texto) {
  lcd.setCursor(0, fila);
  byte n = 0;
  while (texto[n] != '\0' && n < 16) { lcd.print(texto[n]); n++; }
  while (n < 16) { lcd.print(' '); n++; }
}

void actualizarLCD() {
  char linea[17];

  char flecha = (dir == 1) ? '^' : (dir == -1 ? 'v' : '-');
  snprintf(linea, sizeof(linea), "Piso %d %c   %s",
           pisoActual, flecha, modoLOOK ? "LOOK" : "FCFS");
  imprimirLinea(0, linea);

  int pendientes = 0;
  for (int i = 1; i <= PISOS; i++)
    if (cabina[i] || subir[i] || bajar[i]) pendientes++;

  snprintf(linea, sizeof(linea), "Rec:%02d %-6s P%d",
           recorridos, estado == PUERTA ? "PUERTA" : "", pendientes);
  imprimirLinea(1, linea);
}
