#pragma once
#include <Arduino.h>
namespace XOLED{
  void clearScreen();
  void clearLine(uint8_t line);
  void displayLine(uint8_t line,const char *text);
  void displayLine(uint8_t line,const String &text);
  bool isOn();
  void powerUp();
  void powerDown();
  void blank(bool blanked);
  void redraw();
  void solidLine(uint8_t line);
}
