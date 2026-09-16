#include "XCredentials.h"
#include "XRadio.h"
#include "XPolicy.h"
#include "XUsb.h"
#include <RadioLib.h>
#if RADIOLIB_VERSION_MAJOR != 7 || RADIOLIB_VERSION_MINOR != 7 || RADIOLIB_VERSION_PATCH != 1
#error "Review session persistence and StationNode adapter before changing RadioLib 7.7.1"
#endif
#include <Preferences.h>
#include <limits.h>

// Adapter for RadioLib 7.7.1's protected counter and retransmission settings.
// Keep this version pinned; getFCntUp() exposes the last sent, not the next counter.
class StationNode : public LoRaWANNode {
 public:
  using LoRaWANNode::LoRaWANNode;
  uint8_t pending[255]={};
  uint8_t pendingLength=0;
  uint32_t nextCounter()const{return fCntUp;}
  void advanceTo(uint32_t minimum){if(fCntUp<minimum) fCntUp=minimum;}
  void migrateReceiveSettings(){
    // Live TTN session verified 2026-09-16: RX1=5s, RX2=869.525 MHz / DR3.
    // The old firmware retained these only in RAM; preserve them on first migration.
    uint8_t delay=5;
    uint32_t freq=8695250; // LoRaWAN uses 100 Hz units.
    uint8_t rx[4]={3,(uint8_t)freq,(uint8_t)(freq>>8),(uint8_t)(freq>>16)};
    execMacCommand(RADIOLIB_LORAWAN_MAC_RX_TIMING_SETUP,&delay,1);
    execMacCommand(RADIOLIB_LORAWAN_MAC_RX_PARAM_SETUP,rx,4);
  }
  int16_t sendReceive(const uint8_t* up,size_t len,uint8_t port,uint8_t* down,size_t* downLen,
      bool confirmed=false,LoRaWANEvent_t* eventUp=nullptr,LoRaWANEvent_t* eventDown=nullptr) override {
    if(inFlight){
      // RadioLib can otherwise send immediate MAC-only replies from parseDownlink().
      // Save them for the next scheduled slot instead of making extra transmissions.
      if(port==0 && len<=sizeof(pending)){
        memcpy(pending,up,len);pendingLength=len;
        XUsb::log("MAC reply deferred to next scheduled slot: %u bytes",(unsigned)len);
      }
      return RADIOLIB_ERR_UPLINK_UNAVAILABLE;
    }
    nbTrans=1; // One physical transmission per scheduled slot; no confirmed uplink retries.
    isMACPayload=(port==0);
    inFlight=true;
    int16_t result=LoRaWANNode::sendReceive(up,len,port,down,downLen,confirmed,eventUp,eventDown);
    inFlight=false;
    return result;
  }
 private:
  bool inFlight=false;
};
static Preferences prefs;
static SX1262 radio=new Module(8,14,12,13);
static StationNode node(&radio,&EU868,1);
static constexpr uint32_t devAddr=0x260B7D54;
static const uint8_t nwkSKey[16]=CHIRK_ABP_NWK_SKEY;
static const uint8_t appSKey[16]=CHIRK_ABP_APP_SKEY;
static bool isReady=false,prefsOpen=false,storageOK=true;
static bool sentThisBoot=false;
static uint32_t lastAttempt=0,attempts=0,completed=0,quietSec=XPolicy::normalSeconds;
static char statusText[48]="radio not started";
static int16_t lastResult=-999;
static constexpr uint32_t recordMagic=0x43575232;
struct Record {
  uint32_t magic;
  uint32_t reservedNext;
  uint32_t quietSeconds;
  uint8_t nonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
  uint8_t session[RADIOLIB_LORAWAN_SESSION_BUF_SIZE];
  uint8_t pendingLength;
  uint8_t pending[255];
};
static bool openStorage(){
  if(!prefsOpen) prefsOpen=prefs.begin("lorawan",false);
  return prefsOpen;
}
static bool saveState(uint32_t reservedNext){
  Record record={};
  record.magic=recordMagic;record.reservedNext=reservedNext;record.quietSeconds=quietSec;
  memcpy(record.nonces,node.getBufferNonces(),sizeof(record.nonces));
  memcpy(record.session,node.getBufferSession(),sizeof(record.session));
  record.pendingLength=node.pendingLength;
  memcpy(record.pending,node.pending,sizeof(record.pending));
  // One NVS blob is committed atomically. Reserve the next counter BEFORE radio use.
  bool ok=prefs.putBytes("state_v2",&record,sizeof(record))==sizeof(record);
  if(!ok){storageOK=false;snprintf(statusText,sizeof(statusText),"storage fail; TX stopped");}
  return ok;
}
namespace XRadio {
uint32_t sleepSeconds(){return quietSec;}
uint32_t recoverySeconds(){
  if(!openStorage()) return 3600;
  Record record={};
  if(prefs.getBytesLength("state_v2")==sizeof(record) && prefs.getBytes("state_v2",&record,sizeof(record))==sizeof(record)
      && record.magic==recordMagic && record.quietSeconds>=600 && record.quietSeconds<=86400)
    return record.quietSeconds;
  return 600;
}
bool begin(){
  if(isReady) return storageOK;
  if(!openStorage()){snprintf(statusText,sizeof(statusText),"storage unavailable");return false;}
  int16_t state=radio.begin();
  XUsb::log("RADIO radio.begin result=%d",state);
  if(state!=0){snprintf(statusText,sizeof(statusText),"radio fail %d",state);return false;}
  state=node.beginABP(devAddr,nullptr,nullptr,nwkSKey,appSKey);
  if(state!=0){snprintf(statusText,sizeof(statusText),"ABP init %d",state);return false;}
  uint32_t next=0;
  bool migrating=false;
  if(prefs.isKey("state_v2")){
    Record record={};
    if(prefs.getBytesLength("state_v2")!=sizeof(record) || prefs.getBytes("state_v2",&record,sizeof(record))!=sizeof(record)
        || record.magic!=recordMagic || record.quietSeconds<600 || record.quietSeconds>86400){
      snprintf(statusText,sizeof(statusText),"invalid saved state; TX stopped");return false;
    }
    state=node.setBufferNonces(record.nonces);
    if(state==0) state=node.setBufferSession(record.session);
    if(state!=0){snprintf(statusText,sizeof(statusText),"restore fail %d; TX stopped",state);return false;}
    next=record.reservedNext;quietSec=record.quietSeconds;
    node.pendingLength=record.pendingLength;memcpy(node.pending,record.pending,sizeof(node.pending));
    XUsb::log("SESSION restored reserved_next=%lu",(unsigned long)next);
  }else if(prefs.isKey("fcntup")){
    uint32_t legacy=prefs.getUInt("fcntup",0);
    if(legacy>UINT32_MAX-32){snprintf(statusText,sizeof(statusText),"counter exhausted");return false;}
    migrating=true;
    next=legacy+32; // One-time migration from old last-sent counter, allowing an interrupted old save.
    XUsb::log("SESSION legacy migration last=%lu next=%lu",(unsigned long)legacy,(unsigned long)next);
  }else{
    snprintf(statusText,sizeof(statusText),"no saved ABP state; TX stopped");return false;
  }
  state=node.activateABP();
  if(state!=0 && state!=RADIOLIB_LORAWAN_NEW_SESSION && state!=RADIOLIB_LORAWAN_SESSION_RESTORED){
    snprintf(statusText,sizeof(statusText),"ABP activate %d",state);return false;
  }
  if(migrating) node.migrateReceiveSettings();
  node.advanceTo(next);
  node.setADR(false);
  state=node.setDatarate(3); // SF9/BW125, already demonstrated on this installation.
  node.setDutyCycle(true,1000); // 1 second/hour = 24 seconds/day, plus sleep-side enforcement.
  if(state!=0 || !saveState(node.nextCounter())) return false;
  isReady=node.isActivated();
  snprintf(statusText,sizeof(statusText),"%s",isReady?"ABP restored":"ABP inactive");
  return isReady;
}
bool joined(){return isReady;}
int16_t send(uint8_t* payload,uint8_t len){
  if(!isReady || !storageOK) return -999;
  if(sentThisBoot && (uint32_t)(millis()-lastAttempt)<quietSec*1000UL) return RADIOLIB_ERR_UPLINK_UNAVAILABLE;
  uint32_t counter=node.nextCounter();
  if(counter==UINT32_MAX){snprintf(statusText,sizeof(statusText),"counter exhausted");return -999;}
  uint8_t pendingCopy[255];
  uint8_t pendingLen=node.pendingLength;
  memcpy(pendingCopy,node.pending,sizeof(pendingCopy));
  uint8_t port=pendingLen?0:2;
  uint8_t* data=pendingLen?pendingCopy:payload;
  uint8_t count=pendingLen?pendingLen:len;
  uint32_t estimate=XPolicy::airtimeMs(count+13+node.getMacUplinkLen());
  quietSec=XPolicy::quietSeconds(estimate);
  if(!saveState(counter+1)){XUsb::log("ERROR cannot reserve counter; no transmission");return -999;}
  node.pendingLength=0;
  sentThisBoot=true;lastAttempt=millis();++attempts;
  LoRaWANEvent_t up={},down={};uint8_t downlink[255];size_t downLen=sizeof(downlink);
  XUsb::log("TX counter=%lu port=%u bytes=%u reserved_next=%lu quiet_s=%lu",
    (unsigned long)counter,port,count,(unsigned long)(counter+1),(unsigned long)quietSec);
  int16_t state=node.sendReceive(data,count,port,downlink,&downLen,false,&up,&down);
  lastResult=state;
  node.advanceTo(counter+1); // Even uncertain/failed transmissions never reuse a reserved counter.
  uint32_t measured=node.getLastToA();
  uint32_t measuredQuiet=XPolicy::quietSeconds(measured);
  if(measuredQuiet>quietSec) quietSec=measuredQuiet;
  if(state<0 && pendingLen && !node.pendingLength){node.pendingLength=pendingLen;memcpy(node.pending,pendingCopy,pendingLen);}
  bool saved=saveState(node.nextCounter());
  if(state>=0){
    ++completed;
    if(saved) snprintf(statusText,sizeof(statusText),"local tx %lu",(unsigned long)counter);
    XUsb::log("UPLINK counter=%lu library_event_counter=%lu DR=%u freq_MHz=%.3f airtime_ms=%lu sleep_s=%lu",
      (unsigned long)counter,(unsigned long)up.fCnt,up.datarate,up.freq,(unsigned long)measured,(unsigned long)quietSec);
    if(state>0) XUsb::log("DOWNLINK window=%d port=%u counter=%lu RSSI=%d bytes=%u",state,down.fPort,(unsigned long)down.fCnt,down.power,(unsigned)downLen);
    else XUsb::log("RX no downlink; local TX success is not proof of TTN acceptance");
  }else if(saved) snprintf(statusText,sizeof(statusText),"tx fail %d",state);
  XUsb::log("SESSION saved=%u next_counter=%lu result=%d",saved,(unsigned long)node.nextCounter(),state);
  return state;
}
void sleep(){if(isReady) radio.sleep();}
const char* status(){return statusText;}
void report(){
  XUsb::log("RADIO active=%u status=%s addr=%08lX attempts=%lu local_successes=%lu result=%d",
    isReady,statusText,(unsigned long)devAddr,(unsigned long)attempts,(unsigned long)completed,lastResult);
  XUsb::log("RADIO next_counter=%lu persistence=NVS reserved-before-TX storage_ok=%u pending_MAC=%u quiet_s=%lu ADR=off DR=3 duty_cycle=on",
    (unsigned long)node.nextCounter(),storageOK,node.pendingLength,(unsigned long)quietSec);
}
}
