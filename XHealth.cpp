#include "XHealth.h"
#include "XUsb.h"
#include <esp_task_wdt.h>
namespace { bool subscribed=false; }
namespace XHealth {
void begin(){
  esp_task_wdt_config_t config={};
  config.timeout_ms=60000;config.idle_core_mask=0;config.trigger_panic=true;
  esp_err_t result=esp_task_wdt_init(&config);
  if(result==ESP_ERR_INVALID_STATE) result=esp_task_wdt_reconfigure(&config);
  if(result==ESP_OK){
    if(esp_task_wdt_status(nullptr)!=ESP_OK) result=esp_task_wdt_add(nullptr);
    subscribed=(result==ESP_OK);
  }
  XUsb::log("WATCHDOG enabled=%u timeout_s=60 result=%d",subscribed,(int)result);
}
void feed(){if(subscribed) esp_task_wdt_reset();}
void beforeSleep(){if(subscribed){esp_task_wdt_delete(nullptr);subscribed=false;}}
}
