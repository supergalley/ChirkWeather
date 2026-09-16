#pragma once
#include <stdint.h>
#include <math.h>
namespace XPolicy {
constexpr uint32_t normalSeconds=600;
constexpr uint32_t lowBatterySeconds=3600;
constexpr float stopVolts=3.62f;
constexpr float resumeVolts=3.80f;
inline bool batteryLow(float volts,bool latched){
  return !isfinite(volts) || volts<2.5f || volts>4.6f || volts<(latched?resumeVolts:stopVolts);
}
// SF9/BW125, CR4/5, explicit header, CRC, 8-symbol preamble; rounded up.
inline uint32_t airtimeMs(uint32_t frameBytes){
  uint32_t groups=(8*frameBytes+8+35)/36;
  return (uint32_t)ceil((8+4.25+8+5*groups)*4.096);
}
// Target <=24 seconds/day, leaving room below TTN's 30-second allowance.
inline uint32_t quietSeconds(uint32_t toaMs){
  uint32_t seconds=(uint32_t)(((uint64_t)toaMs*3600+999)/1000);
  return seconds>normalSeconds?seconds:normalSeconds;
}
}
