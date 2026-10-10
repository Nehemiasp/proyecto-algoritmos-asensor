#pragma once
#include "Arduino.h"
struct LiquidCrystal_I2C : Printer {
  std::string rows[2] = {std::string(16,' '), std::string(16,' ')}; int r = 0, c = 0;
  LiquidCrystal_I2C(int, int, int) { serial = false; }
  void init() {} void backlight() {}
  void clear() { rows[0] = rows[1] = std::string(16,' '); r = c = 0; }
  void setCursor(int cc, int rr) { c = cc; r = rr; }
  void put(const std::string &s) { for (char ch : s) { if (c < 16) rows[r][c] = ch; c++; } }
};
