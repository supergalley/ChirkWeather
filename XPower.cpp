#include <Arduino.h>
#include "XPins.h"
#include "XPower.h"
namespace { bool enabled = false; }
namespace XPower {
bool converterAvailable() {
#if defined(CONFIG_IDF_TARGET_ESP32S3) && ARDUINO_USB_CDC_ON_BOOT && (PIN_12V_EN == 19 || PIN_12V_EN == 20)
  return false; // Native USB owns these pins. Do not even call pinMode on them.
#else
  return true;
#endif
}
void begin() {
  if (!converterAvailable()) return;
  digitalWrite(PIN_12V_EN, LOW);
  pinMode(PIN_12V_EN, OUTPUT);
}
bool setConverter(bool on) {
  if (!converterAvailable()) return false;
  digitalWrite(PIN_12V_EN, on ? HIGH : LOW);
  enabled = on;
  return true;
}
bool converterOn() { return enabled; }
int converterPinLevel() { return converterAvailable() ? digitalRead(PIN_12V_EN) : -1; }
}
