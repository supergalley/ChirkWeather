#include <cassert>
#include <iostream>
#include "../ChirkWeather.ino"
unsigned long fakeTime=0;
int gpio[49]={},pinWrites[49]={};
int wakeReason=0;
SerialMock Serial;
ESPMock ESP;
int sends=0,displayUpdates=0;
bool displayOn=false,inRadio=false;
namespace XSensors {
 void begin(){}
 bool read(XReadings& r){r={};r.batteryV=3.0f;r.tempC=12;r.humidity=70;r.pressureHpa=1010;return true;}
 const char* lastError(){return "";}
}
namespace XOLED {
 void powerUp(){displayOn=true;}
 void powerDown(){displayOn=false;}
 void blank(bool){}
 void displayLine(uint8_t,const char*){assert(!inRadio);++displayUpdates;}
}
namespace XRadio {
 bool begin(){return true;}
 int16_t send(uint8_t*,uint8_t){inRadio=true;++sends;fakeTime+=2500;inRadio=false;return 0;}
 void report(){}
 const char* status(){return "mock";}
}
void flush(){for(int i=0;i<2000;++i) XUsb::poll(commandReceived);}
void command(const std::string& s){Serial.feed(s+"\n");flush();}
int main(){
 gpio[PIN_BUTTON]=HIGH;
 setup();
 assert(diagnosticMode && displayOn); // A low battery must not force diagnostic sleep.
 command("sample");assert(readings.batteryV==3.0f && diagnosticMode);
 #if PIN_12V_EN == 19
 assert(!XPower::converterAvailable());
 command("boost on");command("boost off");
 assert(pinWrites[19]==0 && pinWrites[20]==0); // Native USB pins are never touched.
 #else
 assert(XPower::converterAvailable() && XPower::converterOn());
 command("boost off");assert(!XPower::converterOn() && gpio[PIN_12V_EN]==LOW);
 command("boost on");assert(XPower::converterOn() && gpio[PIN_12V_EN]==HIGH);
 #endif
 command("auto off");
 command("read");assert(warming);
 command("status");assert(warming); // USB still works during sensor warm-up.
 fakeTime+=sensorWarmupMs;loop();assert(!warming && sampleValid);
 command("tx");assert(warming);
 fakeTime+=sensorWarmupMs;loop();assert(sends==1);
 command("tx");assert(!warming && sends==1); // Rate limit manual repeats.
 fakeTime+=minimumTxIntervalMs;
 command("tx");assert(warming);
 command("cancel");fakeTime+=sensorWarmupMs;loop();assert(sends==1);
 command("interval 5");assert(diagCycleMs==120000);
 command("interval -1");assert(diagCycleMs==120000);
 command("interval 120junk");assert(diagCycleMs==120000);
 command("interval 300");assert(diagCycleMs==300000);
 command("stream 0");assert(streamMs==0);
 command("oled off");assert(oledBlanked && displayOn);
 command("oled on");assert(!oledBlanked && displayOn);
 command("  StAtUs  ");
 // An oversized command must not execute its valid-looking prefix.
 command("boost off"+std::string(120,' '));
 #if PIN_12V_EN != 19
 assert(XPower::converterOn());
 #endif
 assert(Serial.output.find("command too long")!=std::string::npos);
 Serial.connected=false;
 for(int i=0;i<500;++i){XUsb::log("history %d",i);XUsb::poll(commandReceived);}
 Serial.connected=true;flush();
 assert(Serial.output.find("history 499")!=std::string::npos);
 command("logs");
 // Missing environmental readings encode deterministic placeholders plus the error bit.
 XReadings missing={};missing.tempC=NAN;missing.humidity=NAN;missing.pressureHpa=NAN;missing.sensorError=true;
 uint8_t bytes[4];XanderByte::encode(missing,bytes);
 assert(bytes[3]&0x80);
 #if PIN_12V_EN != 19
 // Timer wake retains normal mode, while a fresh reset returns to diagnostics.
 try {command("mode normal");assert(false);}catch(const std::runtime_error& e){assert(std::string(e.what())=="sleep");}
 assert(normalResumeCookie==normalCookie && !XPower::converterOn());
 wakeReason=ESP_SLEEP_WAKEUP_TIMER;
 try {setup();assert(false);}catch(const std::runtime_error& e){assert(std::string(e.what())=="sleep");}
 assert(!diagnosticMode);
 wakeReason=0;setup();assert(diagnosticMode && displayOn);
 #endif
 std::cout<<"Diagnostic control tests passed, converter GPIO "<<PIN_12V_EN<<"\n";
}
