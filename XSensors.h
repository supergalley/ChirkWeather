#pragma once
#include <Arduino.h>
#include "XReadings.h"
namespace XSensors{
  void begin();
  void beginADC();
  void readBattery(uint16_t &raw,float &volts);
  void sleep();
  bool read(XReadings &r);
  const char* dirText(uint8_t code);
  const char* lastError();
}