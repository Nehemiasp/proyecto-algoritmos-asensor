#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
typedef uint8_t byte;
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define OUTPUT 1
struct __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
void pinMode(int, int);
void digitalWrite(int, int);
int digitalRead(int);
unsigned long millis();
unsigned long micros();
void delayMicroseconds(unsigned int);
struct Printer {
  std::string *line = nullptr; int *col = nullptr; bool serial = true;
  void out(const std::string &s);
  void print(const char *s) { out(s); }
  void print(const __FlashStringHelper *s) { out((const char*)s); }
  void print(int v) { out(std::to_string(v)); }
  void print(long v) { out(std::to_string(v)); }
  void println(const char *s) { out(s); out("\n"); }
  void println(const __FlashStringHelper *s) { out((const char*)s); out("\n"); }
  void println(int v) { out(std::to_string(v)); out("\n"); }
  void println(long v) { out(std::to_string(v)); out("\n"); }
  void begin(long) {}
};
extern Printer Serial;
