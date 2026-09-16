#include "XCredentials.h"
#include "XWiFiDebug.h"
#include <WiFi.h>
#include <HTTPClient.h>
static const char *WIFI_SSID=CHIRK_WIFI_SSID;
static const char *WIFI_PASS=CHIRK_WIFI_PASSWORD;
static const char *DEBUG_URL=CHIRK_DEBUG_URL;
static String payloadHex(const uint8_t *payload,uint8_t len){
  char buf[4];
  String out="";
  for(uint8_t i=0;i<len;i++){
    sprintf(buf,"%02X",payload[i]);
    out+=buf;
  }
  return out;
}
namespace XWiFiDebug{
  bool send(const XReadings &r,const uint8_t *payload,uint8_t len,const char *errorText){
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID,WIFI_PASS);
    unsigned long start=millis();
    while(WiFi.status()!=WL_CONNECTED && millis()-start<10000){
      delay(250);
    }
    if(WiFi.status()!=WL_CONNECTED){
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      return false;
    }
    HTTPClient http;
    http.begin(DEBUG_URL);
    http.addHeader("Content-Type","application/json");
    String body="{";
    body+="\"device\":\"chirk-weather-v4\",";
    body+="\"battery_v\":"+String(r.batteryV,2)+",";
    body+="\"anem_raw\":"+String(r.anemRaw)+",";
    body+="\"vane_raw\":"+String(r.vaneRaw)+",";
    body+="\"wind_mph\":"+String(r.windMph,1)+",";
    body+="\"wind_dir_code\":"+String(r.windDirCode)+",";
    body+="\"wind_sensor_v\":"+String(r.windSensorV,3)+",";
    body+="\"vane_sensor_v\":"+String(r.vaneSensorV,3)+",";
    body+="\"temp_c\":"+String(r.tempC,1)+",";
    body+="\"humidity_pct\":"+String(r.humidity,0)+",";
    body+="\"pressure_hpa\":"+String(r.pressureHpa,1)+",";
    body+="\"payload_hex\":\""+payloadHex(payload,len)+"\",";
    body+="\"sensor_error\":"+(r.sensorError?String("true"):String("false"))+",";
    body+="\"error_text\":\""+String(errorText?errorText:"")+"\"";
    body+="}";
    int code=http.POST(body);
    http.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(250);
    return code>=200 && code<300;
  }
}