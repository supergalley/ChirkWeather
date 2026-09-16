#pragma once
#define PIN_BUTTON      0    // PROG button (active LOW)
#define PIN_BATTERY_ADC 1
#define PIN_ANEM        4
#define PIN_VANE        5   // Wind vane (ADC)
#define PIN_BME_SCL     6  // ===== BME280 (I2C) =====
#define PIN_BME_SDA     7
#define PIN_OLED_SDA   17
#define PIN_OLED_SCL   18
// Converter enable wire moved to GPIO47 on 2026-09-16. GPIO19/20 belong to USB.
// XPower refuses to drive GPIO19/20 in a native USB build.
#ifndef PIN_12V_EN
#define PIN_12V_EN     47   // HIGH = ON, LOW = OFF
#endif
#define PIN_OLED_RST   21
#define PIN_VEXT       36
#define PIN_ADC_CTRL   37
