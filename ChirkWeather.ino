/*
  4-byte ChirkWeather payload
  Encoded fields:
  W = wind mph, 0..63                           (B1.0,B1.1,B1.2,B1.3,B1.4,B1.5)
  D = direction code, 0..7  N,NE,E,SE,S,SW,W,NW (B1.6,B1.7,B2.0)
  T = temperature + 20, 0..63                   (B2.1,B2.2,B2.3,B2.4,B2.5,B2.6)
  H = humidity, 0-127%, 1% Steps                (B2.7,B3.0,B3.1,B3.2,B3.3,B3.4,B3.5)
  P = pressure hPa - 980, 0..63                 (B3.6,B3.7,B4.0,B4.1,B4.2,B4.3)
  B = battery code, 0..7                        (B4.4,B4.5,B4.6)
  E = sensor error flag, 0/1                    (B4.7)
  Byte 1:
    bits 1-6  = WindSpeed 0-63mph
    bits 7-8  = Direction bits 1-2 of 3
  Byte 2:
    bit  1    = Direction bit   3 of 3
    bits 2-7  = T -20 to +63c   6 of 6
    bit  8    = Humidity bit    1 of 7
  Byte 3:
    bits 1-6  = Humidity bits 2-7 of 7
    bits 7-8  = Pressure bits 1-2 of 6
  Byte 4:
    bits 1-4  = Pressure bits 3-6 of 6
    bits 5-7  = Battery Level 1-2 of 2 (Levels 1-8) 1=(<=3.6V) 2=(3.7V) 3=(3.8V) 4=(3.9V) 5=(4.0V) 6=(4.1V) 7=(4.2V) 8=(>=4.3V)
    bit  8    = Error present if "1"
*/


#include "XPins.h"
#include "XOLED.h"
#include "XSensors.h"
#include "XReadings.h"
#include "XanderByte.h"
#include "XRadio.h"
#include "XPower.h"
#include "XUsb.h"
#include "XPolicy.h"
#include "XHealth.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

XReadings readings={};
uint8_t payload[4]={};
bool diagnosticMode=false; // Production default. Enter diagnostics explicitly during the boot window.
constexpr uint32_t sensorWarmupMs=5000;
constexpr uint32_t minimumTxIntervalMs=XPolicy::normalSeconds*1000UL;
RTC_DATA_ATTR bool lowBatteryLatched=false;
uint32_t requestedSleepSeconds=XPolicy::normalSeconds;
bool sensorsStarted=false;
uint32_t diagnosticTxNotBefore=0;
bool sampleValid=false, warming=false, sendAfterSample=false;
bool autoTransmit=true, txAttempted=false, oledBlanked=false;
uint32_t warmupStarted=0, lastSample=0, lastTx=0, nextAutomatic=0;
uint32_t diagCycleMs=600000, streamMs=5000, lastStream=0;
bool lastButton=HIGH, stableButton=HIGH;
uint32_t buttonChanged=0;
const char* phase="starting";

bool buttonPressed(){
  bool state=digitalRead(PIN_BUTTON);
  if(state!=lastButton){lastButton=state;buttonChanged=millis();}
  if(millis()-buttonChanged>=80 && state!=stableButton){
    stableButton=state;
    return state==LOW;
  }
  return false;
}
void displayStatus(const char* status){
  if(!diagnosticMode || oledBlanked) return;
  char line[40];
  XOLED::displayLine(0,"CHIRK USB DIAGNOSTIC");
  if(sampleValid){
    snprintf(line,sizeof(line),"Batt:%.2fV raw:%u",readings.batteryV,readings.batteryRaw);
    XOLED::displayLine(1,line);
    snprintf(line,sizeof(line),"T:%.1fC H:%.0f%%",readings.tempC,readings.humidity);
    XOLED::displayLine(2,line);
    snprintf(line,sizeof(line),"Wind:%.1f dir:%u",readings.windMph,readings.windDirCode);
    XOLED::displayLine(3,line);
  }
  snprintf(line,sizeof(line),"12V:%s G%d",!XPower::converterAvailable()?"USB CONFLICT":XPower::converterOn()?"EN ON":"EN OFF",PIN_12V_EN);
  XOLED::displayLine(4,line);
  XOLED::displayLine(5,status);
}
void reportReadings(){
  if(!sampleValid){XUsb::log("SENSORS no sample yet; use read");return;}
  XUsb::log("SENSORS age_ms=%lu batt_V=%.3f batt_raw=%u temperature_C=%.2f humidity_pct=%.2f pressure_hPa=%.2f",
    millis()-lastSample,readings.batteryV,readings.batteryRaw,readings.tempC,readings.humidity,readings.pressureHpa);
  XUsb::log("WIND mph=%.2f direction=%u anem_raw=%u vane_raw=%u anem_V=%.3f vane_V=%.3f converter_command_at_sample=%u",
    readings.windMph,readings.windDirCode,readings.anemRaw,readings.vaneRaw,readings.windSensorV,readings.vaneSensorV,readings.dc12vActive);
  XUsb::log("PAYLOAD %02X %02X %02X %02X sensor_error=%u detail=%s",
    payload[0],payload[1],payload[2],payload[3],readings.sensorError,XSensors::lastError());
}
void reportStatus(){
  XUsb::log("STATUS mode=%s phase=%s uptime_ms=%lu heap=%lu reset_reason=%d wake_reason=%d",
    diagnosticMode?"diagnostic":"normal",phase,millis(),(unsigned long)ESP.getFreeHeap(),(int)esp_reset_reason(),(int)esp_sleep_get_wakeup_cause());
  XUsb::log("POWER converter_GPIO=%d available=%u commanded_on=%u pin_level=%d OLED=%s Vext_level=%d battery_sleep_bypassed=%u",
    PIN_12V_EN,XPower::converterAvailable(),XPower::converterOn(),XPower::converterPinLevel(),oledBlanked?"blank":"on",digitalRead(PIN_VEXT),diagnosticMode);
  XUsb::log("SCHEDULE auto=%u interval_s=%lu stream_s=%lu warming=%u; no 12V output voltage sensor installed",
    autoTransmit,(unsigned long)(diagCycleMs/1000),(unsigned long)(streamMs/1000),warming);
  XRadio::report();
  reportReadings();
}
void sampleNow(){
  bool ok=XSensors::read(readings);
  XanderByte::encode(readings,payload);
  sampleValid=true;lastSample=millis();
  XUsb::log("READ finished ok=%u converter_command=%u",ok,XPower::converterOn());
  reportReadings();
}
bool txAllowed(){
  if((int32_t)(millis()-diagnosticTxNotBefore)<0){
    XUsb::log("TX recovery holdoff: wait %lu seconds",(unsigned long)((diagnosticTxNotBefore-millis()+999)/1000));
    return false;
  }
  if(txAttempted && millis()-lastTx<minimumTxIntervalMs){
    XUsb::log("TX deferred: wait %lu seconds",(minimumTxIntervalMs-(millis()-lastTx)+999)/1000);
    return false;
  }
  return true;
}
void transmit(){
  if(!txAllowed()) return;
  txAttempted=true;lastTx=millis();
  phase="radio TX/RX";
  displayStatus("TX/RX in progress");
  // USB commands and OLED refresh pause here until RadioLib finishes both RX windows.
  bool ready=XRadio::begin();
  int16_t result=ready?XRadio::send(payload,sizeof(payload)):-999;
  XUsb::log("CYCLE radio_ready=%u result=%d detail=%s",ready,result,XRadio::status());
  phase="idle";
  displayStatus(result>=0?"TX local OK":"TX failed; USB logs");
  uint32_t quietMs=XRadio::sleepSeconds()*1000UL;
  diagnosticTxNotBefore=millis()+quietMs;
  nextAutomatic=millis()+(quietMs>diagCycleMs?quietMs:diagCycleMs);
}
void startMeasurement(bool thenSend){
  if(warming){XUsb::log("ERROR measurement already warming; use cancel");return;}
  if(thenSend && !txAllowed()) return;
  warming=true;sendAfterSample=thenSend;warmupStarted=millis();phase="sensor warm-up";
  XUsb::log("READ warming %lu ms; converter_command=%u available=%u (use boost on to enable)",
    (unsigned long)sensorWarmupMs,XPower::converterOn(),XPower::converterAvailable());
  displayStatus("Sensor warm-up");
}
void help(){
  XUsb::log("COMMANDS: help | status | pins | read | sample | radio | tx | cancel | logs");
  XUsb::log("POWER: boost on | boost off | oled on | oled off (blanks display; keeps Vext powered)");
  XUsb::log("CONTROL: auto on|off | interval 600..3600 (seconds) | stream 0..3600 (0=off)");
  XUsb::log("SERVICE: sensors retry | mode normal | mode diagnostic | reboot");
  XUsb::log("read=5-second warm-up then sample; sample=immediate; tx=fresh sample then unconfirmed uplink");
  XUsb::log("TX is limited to one attempt per 600 seconds (or longer for airtime); success is local, not proof of TTN acceptance");
  XUsb::log("USB commands queue during radio TX/RX; recent logs are retained in RAM, never keys");
}
bool secondsArgument(const char* text,uint32_t low,uint32_t high,uint32_t& output){
  if(!*text) return false;
  for(const char* c=text;*c;++c) if(!isdigit((unsigned char)*c)) return false;
  char* end;unsigned long value=strtoul(text,&end,10);
  if(*end || value<low || value>high) return false;
  output=(uint32_t)value*1000;return true;
}
void drainBriefly(){
  uint32_t start=millis();
  while(millis()-start<80){XUsb::drain();delay(1);}
}
void enterDiagnostic(){
  diagnosticMode=true;
  XOLED::powerUp();
  XPower::setConverter(true);
  XUsb::log("MODE diagnostic explicitly requested; battery sleep bypassed");
}
void bootCommand(const char* cmd){
  if(!strcmp(cmd,"mode diagnostic")) enterDiagnostic();
  else XUsb::log("BOOT: send 'mode diagnostic' or press PROG after RESET to stay awake");
}
void sleepNormal(){
  XPower::setConverter(false);
  if(sensorsStarted) XSensors::sleep();
  XRadio::sleep();
  XOLED::powerDown();
  XPower::prepareSleep();
  uint32_t quiet=XRadio::sleepSeconds();
  if(requestedSleepSeconds<quiet) requestedSleepSeconds=quiet;
  XUsb::log("SLEEP seconds=%lu low_battery=%u; converter/OLED/radio off",(unsigned long)requestedSleepSeconds,lowBatteryLatched);
  drainBriefly();
  XHealth::beforeSleep();
  esp_sleep_enable_timer_wakeup((uint64_t)requestedSleepSeconds*1000000ULL);
  esp_deep_sleep_start();
}
void normalCycle(){
  phase="normal battery check";
  XPower::setConverter(false);
  XSensors::readBattery(readings.batteryRaw,readings.batteryV);
  lowBatteryLatched=XPolicy::batteryLow(readings.batteryV,lowBatteryLatched);
  XUsb::log("BATTERY measured_V=%.3f raw=%u low_latched=%u",readings.batteryV,readings.batteryRaw,lowBatteryLatched);
  XHealth::feed();
  if(lowBatteryLatched){requestedSleepSeconds=XPolicy::lowBatterySeconds;sleepNormal();}
  if(!sensorsStarted){
    XUsb::log("BOOT BME280 initialization starting");drainBriefly();
    XSensors::begin();sensorsStarted=true;XHealth::feed();
  }
  XPower::setConverter(true);
  uint32_t started=millis();
  while(millis()-started<sensorWarmupMs){
    if(buttonPressed()) enterDiagnostic();
    XUsb::poll(bootCommand);
    XHealth::feed();delay(10);
  }
  sampleNow();XHealth::feed();
  // Check loaded voltage as well; converter start-up can reveal a weak battery.
  if(!diagnosticMode && XPolicy::batteryLow(readings.batteryV,false)){
    lowBatteryLatched=true;requestedSleepSeconds=XPolicy::lowBatterySeconds;sleepNormal();
  }
  if(!diagnosticMode) XPower::setConverter(false);
  transmit();XHealth::feed();
  if(!diagnosticMode) sleepNormal();
}
void commandReceived(const char* input){
  char cmd[96];size_t len=strlen(input);
  while(len && input[len-1]==' ') --len;
  while(*input==' ' && len){++input;--len;}
  for(size_t i=0;i<len;++i) cmd[i]=tolower((unsigned char)input[i]);
  cmd[len]=0;
  if(!len) return;
  if(!strcmp(cmd,"help") || !strcmp(cmd,"?")) help();
  else if(!strcmp(cmd,"status")) reportStatus();
  else if(!strcmp(cmd,"radio")) XRadio::report();
  else if(!strcmp(cmd,"logs")) XUsb::replay();
  else if(!strcmp(cmd,"pins")){
    XUsb::log("PINS boost=%d battery_ADC=%d ADC_control=%d anem=%d vane=%d BME_SDA=%d BME_SCL=%d",
      PIN_12V_EN,PIN_BATTERY_ADC,PIN_ADC_CTRL,PIN_ANEM,PIN_VANE,PIN_BME_SDA,PIN_BME_SCL);
    XUsb::log("PINS OLED SDA=%d SCL=%d RESET=%d Vext=%d radio NSS=8 DIO1=14 RESET=12 BUSY=13; USB=19/20",
      PIN_OLED_SDA,PIN_OLED_SCL,PIN_OLED_RST,PIN_VEXT);
    XUsb::log("WARN V4 GPIO7 may also control radio FEM power; verify BME SDA wiring against board revision");
  }
  else if(!strcmp(cmd,"read")) startMeasurement(false);
  else if(!strcmp(cmd,"sample")){
    if(warming) XUsb::log("ERROR warming; wait or cancel");
    else {sampleNow();displayStatus("Sample complete");}
  }
  else if(!strcmp(cmd,"tx")) startMeasurement(true);
  else if(!strcmp(cmd,"cancel")){
    warming=false;sendAfterSample=false;phase="idle";
    XUsb::log("OK pending measurement/TX cancelled; auto schedule unchanged");displayStatus("Cancelled");
  }
  else if(!strcmp(cmd,"boost on") || !strcmp(cmd,"boost off")){
    bool on=!strcmp(cmd,"boost on");
    if(!XPower::setConverter(on)) XUsb::log("ERROR GPIO%d is reserved for native USB; move enable wire and update PIN_12V_EN",PIN_12V_EN);
    else{
      if(warming){warming=false;sendAfterSample=false;phase="idle";XUsb::log("Pending measurement cancelled after converter change");}
      XUsb::log("OK converter enable GPIO%d=%s; verify actual output with a meter",PIN_12V_EN,on?"HIGH":"LOW");
    }
    displayStatus(on?"Boost ON requested":"Boost OFF requested");
  }
  else if(!strcmp(cmd,"oled on") || !strcmp(cmd,"oled off")){
    oledBlanked=!strcmp(cmd,"oled off");XOLED::blank(oledBlanked);
    XUsb::log("OK OLED %s; Vext stays on",oledBlanked?"blank":"on");displayStatus("USB diagnostics");
  }
  else if(!strcmp(cmd,"auto on") || !strcmp(cmd,"auto off")){
    autoTransmit=!strcmp(cmd,"auto on");nextAutomatic=millis()+diagCycleMs;
    if(!autoTransmit && sendAfterSample){warming=false;sendAfterSample=false;phase="idle";}
    XUsb::log("OK automatic uplinks %s",autoTransmit?"on":"off");
  }
  else if(!strncmp(cmd,"interval ",9)){
    if(secondsArgument(cmd+9,600,3600,diagCycleMs)){nextAutomatic=millis()+diagCycleMs;XUsb::log("OK interval_s=%lu",(unsigned long)(diagCycleMs/1000));}
    else XUsb::log("ERROR interval must be a whole number 600..3600 seconds");
  }
  else if(!strncmp(cmd,"stream ",7)){
    if(secondsArgument(cmd+7,0,3600,streamMs)){lastStream=millis();XUsb::log("OK stream_s=%lu (cached readings; use read for a fresh sample)",(unsigned long)(streamMs/1000));}
    else XUsb::log("ERROR stream must be a whole number 0..3600 seconds");
  }
  else if(!strcmp(cmd,"sensors retry")){
    if(warming) XUsb::log("ERROR warming; wait or cancel");
    else{XSensors::begin();sensorsStarted=true;sampleNow();displayStatus("Sensors retried");}
  }
  else if(!strcmp(cmd,"mode diagnostic")){XUsb::log("OK already in diagnostic mode; no automatic sleep");}
  else if(!strcmp(cmd,"mode normal")){
    if(!XPower::converterAvailable()){XUsb::log("ERROR fix converter/USB pin conflict before normal operation");return;}
    warming=false;sendAfterSample=false;diagnosticMode=false;sleepNormal();
  }
  else if(!strcmp(cmd,"reboot")){
    XPower::setConverter(false);ESP.restart();
  }
  else XUsb::log("ERROR unknown command; use help");
}
void setup(){
  XUsb::begin();
  XPower::begin();
  pinMode(PIN_BUTTON,INPUT_PULLUP);
  pinMode(PIN_VEXT,OUTPUT);digitalWrite(PIN_VEXT,HIGH);
  XHealth::begin();
  XSensors::beginADC();
  XUsb::log("BOOT ChirkWeather production v2 compiled=" __DATE__ " " __TIME__);
  XUsb::log("BOOT reset=%d wake=%d; normal minimum interval=600 seconds; PROG/USB diagnostic entry available",
    (int)esp_reset_reason(),(int)esp_sleep_get_wakeup_cause());
  requestedSleepSeconds=XRadio::recoverySeconds();
  bool timerWake=(esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_TIMER);
  diagnosticTxNotBefore=timerWake?0:millis()+requestedSleepSeconds*1000UL;
  uint32_t started=millis();
  // A short grace window on every wake; never wait indefinitely for a USB host.
  while(millis()-started<3000 && !diagnosticMode){
    if(buttonPressed()) enterDiagnostic();
    XUsb::poll(bootCommand);XHealth::feed();delay(5);
  }
  if(!diagnosticMode){
    // On power-on, reset, brownout or watchdog recovery, wait a full interval before TX.
    // This prevents reboot loops or repeated resets from flooding the network.
    if(!timerWake){XUsb::log("RECOVERY holdoff before first transmission");sleepNormal();}
    normalCycle();
    if(!diagnosticMode) return;
  }
  if(!sensorsStarted){
    XUsb::log("BOOT BME280 initialization starting");drainBriefly();
    XSensors::begin();sensorsStarted=true;XHealth::feed();
  }
  phase="idle";
  nextAutomatic=millis()+diagCycleMs;
  help();reportStatus();displayStatus("USB ready; type help");
}
void loop(){
  XHealth::feed();
  if(!diagnosticMode){sleepNormal();return;}
  XUsb::poll(commandReceived);
  if(buttonPressed()) {reportStatus();displayStatus("USB diagnostics");}
  if(warming && millis()-warmupStarted>=sensorWarmupMs){
    warming=false;bool send=sendAfterSample;sendAfterSample=false;
    sampleNow();phase="idle";
    if(send) transmit();else displayStatus("Sample complete");
  }
  if(!warming && autoTransmit && (int32_t)(millis()-nextAutomatic)>=0){
    nextAutomatic=millis()+diagCycleMs;
    startMeasurement(true);
  }
  if(streamMs && millis()-lastStream>=streamMs){
    lastStream=millis();
    XUsb::log("LIVE phase=%s heap=%lu boost_command=%u pin=%d sample_age_ms=%ld radio=%s",
      phase,(unsigned long)ESP.getFreeHeap(),XPower::converterOn(),XPower::converterPinLevel(),
      sampleValid?(long)(millis()-lastSample):-1L,XRadio::status());
  }
  delay(1);
}
