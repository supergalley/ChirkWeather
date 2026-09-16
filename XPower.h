#pragma once
namespace XPower {
  void begin();
  bool converterAvailable();
  bool setConverter(bool on);
  bool converterOn();
  int converterPinLevel();
}
