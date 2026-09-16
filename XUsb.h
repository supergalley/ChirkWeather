#pragma once
#include <Arduino.h>
namespace XUsb {
  using CommandHandler = void (*)(const char*);
  void begin();
  void log(const char* format, ...) __attribute__((format(printf, 1, 2)));
  void poll(CommandHandler handler);
  void replay();
  void drain();
}
