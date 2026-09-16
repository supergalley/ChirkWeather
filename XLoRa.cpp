#include "XCredentials.h"
#include "XLoRa.h"
#include "LoRaWan_APP.h"
uint8_t appDataRate=3;
uint8_t devEui[]={0x70,0xB3,0xD5,0x7E,0xD0,0x07,0x6D,0xC4};
uint8_t appEui[]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
uint8_t appKey[]=CHIRK_LEGACY_APP_KEY;
uint8_t nwkSKey[]=CHIRK_LEGACY_NWK_SKEY;
uint8_t appSKey[]=CHIRK_LEGACY_APP_SKEY;
uint32_t devAddr=(uint32_t)0x007E6AE1;
uint16_t userChannelsMask[6]={0x00FF,0x0000,0x0000,0x0000,0x0000,0x0000};
static uint8_t txPayload[16]={0};
static uint8_t txPayloadLen=4;
LoRaMacRegion_t loraWanRegion=ACTIVE_REGION;
DeviceClass_t loraWanClass=CLASS_A;
uint32_t appTxDutyCycle=600000;
bool overTheAirActivation=true;
bool loraWanAdr=true;
bool isTxConfirmed=true;
uint8_t appPort=2;
uint8_t confirmedNbTrials=4;
static void prepareTxFrame(uint8_t port){
  appDataSize=txPayloadLen;
  for(uint8_t i=0;i<txPayloadLen;i++) appData[i]=txPayload[i];
}
namespace XLoRa{
  void begin(){
    pinMode(Vext,OUTPUT);
    #ifdef LORA_PA_POWER
      pinMode(LORA_PA_POWER,ANALOG);
    #endif
    #ifdef LORA_PA_EN
      pinMode(LORA_PA_EN,OUTPUT);
      digitalWrite(LORA_PA_EN,HIGH);
    #endif
    #ifdef LORA_PA_TX_EN
      pinMode(LORA_PA_TX_EN,OUTPUT);
      digitalWrite(LORA_PA_TX_EN,HIGH);
    #endif
    delay(100);
    digitalWrite(Vext,LOW);
    delay(100);
    Mcu.begin(HELTEC_BOARD,SLOW_CLK_TPYE);
    delay(2000);
  }
  void loop(){
    switch(deviceState){
      case DEVICE_STATE_INIT:{
        #if(LORAWAN_DEVEUI_AUTO)
        LoRaWAN.generateDeveuiByChipID();
        #endif
        LoRaWAN.init(loraWanClass,loraWanRegion);
        delay(3000);
        LoRaWAN.setDefaultDR(appDataRate);
        break;
      }
      case DEVICE_STATE_JOIN:{
        LoRaWAN.join();
        delay(5000);
        break;
      }
      case DEVICE_STATE_SEND:{
        prepareTxFrame(appPort);
        LoRaWAN.send();
        deviceState=DEVICE_STATE_CYCLE;
        break;
      }
      case DEVICE_STATE_CYCLE:{
        txDutyCycleTime=appTxDutyCycle+randr(-APP_TX_DUTYCYCLE_RND,APP_TX_DUTYCYCLE_RND);
        LoRaWAN.cycle(txDutyCycleTime);
        deviceState=DEVICE_STATE_SLEEP;
        break;
      }
      case DEVICE_STATE_SLEEP:{
        LoRaWAN.sleep(loraWanClass);
        break;
      }
      default:{
        deviceState=DEVICE_STATE_INIT;
        break;
      }
    }
  }
  void setPayload(const uint8_t *data,uint8_t len){
    if(len>16) len=16;
    txPayloadLen=len;
    for(uint8_t i=0;i<len;i++) txPayload[i]=data[i];
  }
}