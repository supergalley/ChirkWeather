#pragma once
#include <Arduino.h>
namespace XLoRa{
  void begin();
  void setPayload(const uint8_t *data,uint8_t len);
  void loop();
}