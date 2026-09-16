#pragma once
#include <Arduino.h>
#include "XReadings.h"
namespace XanderByte{
  void encode(const XReadings &r,uint8_t out[4]);
  String hex(const uint8_t *b,uint8_t n);
}