#pragma once
#include <Arduino.h>
namespace XRadio{
  bool begin();
  bool joined();
  int16_t send(uint8_t *payload,uint8_t len);
  const char* status();
  void report();
}
