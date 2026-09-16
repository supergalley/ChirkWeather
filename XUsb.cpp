#include "XUsb.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr uint32_t capacity = 64;
constexpr size_t lineSize = 224;
char journal[capacity][lineSize] = {};
uint32_t sequence = 0, nextOutput = 0;
size_t outputOffset = 0;
char command[96];
size_t commandLength = 0;
bool overflow = false;
}
namespace XUsb {
void begin() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setRxBufferSize(1024);
  Serial.setTxTimeoutMs(0);
#endif
}
void log(const char* format, ...) {
  char* line = journal[sequence % capacity];
  int prefix = snprintf(line, lineSize, "[%lu ms] ", (unsigned long)millis());
  va_list args;
  va_start(args, format);
  vsnprintf(line + prefix, lineSize - prefix - 1, format, args);
  va_end(args);
  size_t n = strlen(line);
  line[n] = '\n';
  line[n + 1] = '\0';
  ++sequence;
}
void replay() {
  nextOutput = sequence > 32 ? sequence - 32 : 0;
  outputOffset = 0;
}
void poll(CommandHandler handler) {
  // A disconnected or slow USB host must never hold up the radio or sensors.
  if (Serial && nextOutput != sequence) {
    if (sequence - nextOutput > capacity) {
      nextOutput = sequence - capacity;
      outputOffset = 0;
    }
    const char* line = journal[nextOutput % capacity];
    size_t remaining = strlen(line) - outputOffset;
    int room = Serial.availableForWrite();
    if (room > 0) {
      size_t count = remaining < (size_t)room ? remaining : (size_t)room;
      outputOffset += Serial.write((const uint8_t*)line + outputOffset, count);
      if (outputOffset == strlen(line)) { ++nextOutput; outputOffset = 0; }
    }
  }
  // Bounded work per poll; accept CR, LF, or CRLF. Never execute a truncated command.
  for (unsigned n = 0; n < 96 && Serial.available(); ++n) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (overflow) log("ERROR command too long; discarded");
      else if (commandLength) {
        command[commandLength] = 0;
        handler(command);
      }
      commandLength = 0;
      overflow = false;
      break;
    }
    if (c == '\b' || c == 127) { if (commandLength && !overflow) --commandLength; continue; }
    if (c < 32 || c > 126) continue;
    if (commandLength < sizeof(command)-1 && !overflow) command[commandLength++] = c;
    else overflow = true;
  }
}
}
