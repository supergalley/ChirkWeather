#pragma once
#include <Arduino.h>
#include "XReadings.h"
namespace XWiFiDebug{
  bool send(const XReadings &r,const uint8_t *payload,uint8_t len,const char *errorText);
}