#include "XSensors.h"
#include "XPins.h"
#include "XPower.h"
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <math.h>
static const float VREF=3.30f;
static const float DIV_K=37.0f/22.0f;
static const float ANEM_MAX_V=5.0f;
static const float ANEM_MAX_MS=30.0f;
static const float MS_TO_MPH=2.237f;
static const float ANEM_ZERO_DB_V=0.03f;
static const uint16_t BOOST_SETTLE_MS=500;
static Adafruit_BME280 bme;
static TwoWire BME_I2C(1);
static bool bmeOK=false;
static char errorText[48]="";
struct DirPt{const char* label;uint16_t raw;uint8_t code;};
static const DirPt DIRS[8]={
  {"N",19,0},
  {"NE",492,1},
  {"E",980,2},
  {"SE",1475,3},
  {"S",1977,4},
  {"SW",2477,5},
  {"W",3019,6},
  {"NW",3724,7}
};
static uint16_t readADCoversample(int pin,int n=64){
  uint32_t s=0;
  for(int i=0;i<n;i++) s+=analogRead(pin);
  return s/n;
}
static float adcToVolts(uint16_t counts){
  return (counts*VREF)/4095.0f;
}
static uint8_t mapVaneCode(uint16_t raw){
  uint8_t best=0;
  uint16_t bestDiff=65535;
  for(uint8_t i=0;i<8;i++){
    uint16_t d=abs((int)raw-(int)DIRS[i].raw);
    if(d<bestDiff){bestDiff=d;best=DIRS[i].code;}
  }
  return best;
}

namespace XSensors{
  void begin(){
    errorText[0]='\0';
    pinMode(PIN_ADC_CTRL,OUTPUT);
    digitalWrite(PIN_ADC_CTRL,LOW);
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_ANEM,ADC_11db);
    analogSetPinAttenuation(PIN_VANE,ADC_11db);
    analogSetPinAttenuation(PIN_BATTERY_ADC,ADC_11db);
    BME_I2C.begin(PIN_BME_SDA,PIN_BME_SCL,400000);
    BME_I2C.setTimeOut(50);
    if(bme.begin(0x76,&BME_I2C)){bmeOK=true;return;}
    if(bme.begin(0x77,&BME_I2C)){bmeOK=true;return;}
    bmeOK=false;
    strcpy(errorText,"BME280 not found");
  }
  bool read(XReadings &r){
    bool ok=true;
    r.sensorError=false;
    errorText[0]=0;
    (void)analogRead(PIN_ANEM);
    (void)analogRead(PIN_VANE);
    uint16_t aCounts=readADCoversample(PIN_ANEM,64);
    uint16_t vCounts=readADCoversample(PIN_VANE,64);
    r.anemRaw=aCounts;
    r.vaneRaw=vCounts;
    r.dc12vActive=XPower::converterOn(); // Commanded state, not measured 12 V.
    float vAIn=adcToVolts(aCounts)*DIV_K;
    r.windSensorV=vAIn;
    float vEff=(vAIn<ANEM_ZERO_DB_V)?0.0f:(vAIn-ANEM_ZERO_DB_V);
    float windMs=(vEff/ANEM_MAX_V)*ANEM_MAX_MS;
    if(windMs<0) windMs=0;
    r.windMph=windMs*MS_TO_MPH;
    float vVIn=adcToVolts(vCounts)*DIV_K;
    r.vaneSensorV=vVIn;
    r.windDirCode=mapVaneCode(vCounts);
    digitalWrite(PIN_ADC_CTRL,HIGH);
    delay(10);
    uint16_t bCounts=readADCoversample(PIN_BATTERY_ADC,16);
    int bMv=analogReadMilliVolts(PIN_BATTERY_ADC);
    digitalWrite(PIN_ADC_CTRL,LOW);
    r.batteryRaw=bCounts;
    r.batteryV=(bMv/1000.0f)*4.55f;
    if(bmeOK){
      r.tempC=bme.readTemperature();
      r.humidity=bme.readHumidity();
      r.pressureHpa=bme.readPressure()/100.0f;
      if(!isfinite(r.tempC)||!isfinite(r.humidity)||!isfinite(r.pressureHpa)){
        ok=false;
        r.sensorError=true;
        strcpy(errorText,"BME280 read failed");
      }
    }else{
      strcpy(errorText,"BME280 not found");
      r.tempC=NAN;
      r.humidity=NAN;
      r.pressureHpa=NAN;
      ok=false;
      r.sensorError=true;
    }
    return ok;
  }

  uint16_t readAnemometerRaw(){
    return readADCoversample(PIN_ANEM,64);
  }
  uint16_t readVaneRaw(){
    return readADCoversample(PIN_VANE,64);
  }
  const char* dirText(uint8_t code){
    return DIRS[code&7].label;
  }
  const char* lastError(){
    return errorText;
  }
}
