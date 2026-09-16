#pragma once
namespace XPower {
  void begin();
  void prepareSleep();
  bool converterAvailable();
  bool setConverter(bool on);
  bool converterOn();
  int converterPinLevel();
}
