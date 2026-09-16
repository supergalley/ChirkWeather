#pragma once
#include <Arduino.h>
#include "XReadings.h"
namespace XSensors{
  void begin();
  bool read(XReadings &r);
  const char* dirText(uint8_t code);
  const char* lastError();
}