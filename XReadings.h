#pragma once
#include <Arduino.h>
struct XReadings{
  float tempC;
  float humidity;
  float pressureHpa;
  float windMph;
  float windSensorV;
  float vaneSensorV;
  uint8_t windDirCode;
  float batteryV;
  uint16_t anemRaw;
  uint16_t vaneRaw;
  uint16_t batteryRaw;
  bool sensorError;
  bool dc12vActive;
};