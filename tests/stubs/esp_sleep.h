#pragma once
#include <cstdint>
#include <stdexcept>
#define ESP_SLEEP_WAKEUP_TIMER 4
extern int wakeReason;
inline int esp_sleep_get_wakeup_cause(){return wakeReason;}
inline void esp_sleep_enable_timer_wakeup(uint64_t){}
inline void esp_deep_sleep_start(){throw std::runtime_error("sleep");}
