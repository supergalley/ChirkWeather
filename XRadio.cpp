#include "XCredentials.h"
#include "XRadio.h"
#include "XUsb.h"
#include <RadioLib.h>
#include <Preferences.h>
static Preferences prefs;
static uint32_t uplinkCounter=0;
SX1262 radio=new Module(8,14,12,13);
LoRaWANNode node(&radio,&EU868,1);
static bool isReady=false;
static char statusText[40]="radio not started";
static const uint32_t devAddr=0x260B7D54;
static const uint8_t nwkSKey[16]=CHIRK_ABP_NWK_SKEY;
static const uint8_t appSKey[16]=CHIRK_ABP_APP_SKEY;
static bool prefsOpen=false;
static uint32_t attempts=0;
static uint32_t completed=0;
static int16_t lastResult=-999;

namespace XRadio {
  bool begin() {
    // Keep the active ABP session in RAM throughout diagnostic mode.
    if (isReady) return true;
    if (!prefsOpen) prefsOpen=prefs.begin("lorawan",false);
    uplinkCounter=prefsOpen?prefs.getUInt("fcntup",0):0;
    XUsb::log("RADIO begin EU868 ABP addr=%08lX saved_counter=%lu (not restored by legacy code)",
      (unsigned long)devAddr,(unsigned long)uplinkCounter);
    if(!prefsOpen) XUsb::log("WARN counter storage unavailable");
    int16_t state=radio.begin();
    XUsb::log("RADIO radio.begin result=%d",state);
    if(state!=RADIOLIB_ERR_NONE){snprintf(statusText,sizeof(statusText),"radio fail %d",state);return false;}
    state=node.beginABP(devAddr,NULL,NULL,nwkSKey,appSKey);
    XUsb::log("RADIO beginABP result=%d",state);
    if(state!=RADIOLIB_ERR_NONE){snprintf(statusText,sizeof(statusText),"abp init %d",state);return false;}
    state=node.activateABP();
    XUsb::log("RADIO activateABP result=%d",state);
    if(state!=RADIOLIB_ERR_NONE && state!=RADIOLIB_LORAWAN_NEW_SESSION && state!=RADIOLIB_LORAWAN_SESSION_RESTORED){
      snprintf(statusText,sizeof(statusText),"abp act %d",state);return false;
    }
    node.setADR(false);
    node.setDutyCycle(false); // Preserve existing RF settings; console throttles attempts to >=120s.
    isReady=node.isActivated();
    snprintf(statusText,sizeof(statusText),"%s",isReady?"abp ready":"abp not active");
    XUsb::log("WARN ABP state still not restored after reboot/deep sleep; live counter=%lu",(unsigned long)node.getFCntUp());
    return isReady;
  }
  bool joined(){return isReady;}
  int16_t send(uint8_t* payload,uint8_t len){
    if(!isReady){snprintf(statusText,sizeof(statusText),"not ready");return -999;}
    ++attempts;
    LoRaWANEvent_t eventUp={},eventDown={};
    uint8_t downlink[255];
    size_t downlinkLen=sizeof(downlink);
    XUsb::log("TX start attempt=%lu counter_before=%lu bytes=%u port=2 unconfirmed",
      (unsigned long)attempts,(unsigned long)node.getFCntUp(),len);
    unsigned long started=millis();
    // No USB output or OLED I2C traffic inside the synchronous TX/RX operation.
    int16_t state=node.sendReceive(payload,len,2,downlink,&downlinkLen,false,&eventUp,&eventDown);
    lastResult=state;
    XUsb::log("TX/RX returned result=%d elapsed_ms=%lu counter_after=%lu",
      state,millis()-started,(unsigned long)node.getFCntUp());
    if(state>=0){
      ++completed;
      uplinkCounter=node.getFCntUp();
      if(!prefsOpen || prefs.putUInt("fcntup",uplinkCounter)!=sizeof(uplinkCounter))
        XUsb::log("WARN could not save counter");
      snprintf(statusText,sizeof(statusText),"local tx %lu",(unsigned long)eventUp.fCnt);
      XUsb::log("UPLINK counter=%lu DR=%u freq_MHz=%.3f power_dBm=%d transmissions=%u",
        (unsigned long)eventUp.fCnt,eventUp.datarate,eventUp.freq,eventUp.power,eventUp.nbTrans);
      XUsb::log("TX local success; TTN acceptance must be checked separately");
      if(state>0){
        XUsb::log("DOWNLINK window=%d port=%u counter=%lu RSSI=%d bytes=%u",
          state,eventDown.fPort,(unsigned long)eventDown.fCnt,eventDown.power,(unsigned)downlinkLen);
        for(size_t i=0;i<downlinkLen;i+=24){
          char hex[73];size_t end=(downlinkLen-i<24)?downlinkLen-i:24;
          for(size_t j=0;j<end;++j) snprintf(hex+j*3,4,"%02X ",downlink[i+j]);
          XUsb::log("DOWNLINK data %s",hex);
        }
      }else XUsb::log("RX no downlink (normal for unconfirmed uplinks)");
    }else snprintf(statusText,sizeof(statusText),"tx fail %d",state);
    return state;
  }
  const char* status(){return statusText;}
  void report(){
    XUsb::log("RADIO active=%u status=%s addr=%08lX attempts=%lu local_successes=%lu last_result=%d",
      isReady,statusText,(unsigned long)devAddr,(unsigned long)attempts,(unsigned long)completed,lastResult);
    XUsb::log("RADIO counter_live=%lu counter_saved=%lu persistence=legacy-not-restored ADR=off duty_cycle=legacy-off",
      (unsigned long)node.getFCntUp(),(unsigned long)uplinkCounter);
  }
}
