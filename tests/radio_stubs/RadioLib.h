#pragma once
#include <cstring>
#define RADIOLIB_VERSION_MAJOR 7
#define RADIOLIB_VERSION_MINOR 7
#define RADIOLIB_VERSION_PATCH 1
#define RADIOLIB_LORAWAN_NONCES_BUF_SIZE 16
#define RADIOLIB_LORAWAN_SESSION_BUF_SIZE 128
#define RADIOLIB_ERR_UPLINK_UNAVAILABLE -1108
#define RADIOLIB_LORAWAN_NEW_SESSION -1118
#define RADIOLIB_LORAWAN_SESSION_RESTORED -1117
#define RADIOLIB_LORAWAN_MAC_RX_TIMING_SETUP 8
#define RADIOLIB_LORAWAN_MAC_RX_PARAM_SETUP 5
inline int EU868=0,physicalSends=0;
inline bool powerCut=false,macReply=false;
inline uint32_t lastCounter=0;
struct Module{Module(int,int,int,int){}};
struct SX1262{SX1262(Module*){} int begin(){return 0;} void sleep(){}};
struct LoRaWANEvent_t{uint32_t fCnt=0;uint8_t datarate=3,fPort=2;float freq=868.5;int power=-90;};
class LoRaWANNode {
 protected:
 uint32_t fCntUp=0;uint8_t nbTrans=1;bool isMACPayload=false;
 uint8_t nonces[16]={},session[128]={};
 bool execMacCommand(uint8_t,uint8_t*,uint8_t){return true;}
 public:
 LoRaWANNode(SX1262*,int*,int){}
 virtual ~LoRaWANNode()=default;
 int beginABP(uint32_t,const uint8_t*,const uint8_t*,const uint8_t*,const uint8_t*){fCntUp=0;return 0;}
 int activateABP(){return RADIOLIB_LORAWAN_NEW_SESSION;}
 bool isActivated(){return true;}
 void setADR(bool){} int setDatarate(int){return 0;} void setDutyCycle(bool,int){}
 uint8_t* getBufferNonces(){return nonces;}
 uint8_t* getBufferSession(){memcpy(session,&fCntUp,4);return session;}
 int setBufferNonces(const uint8_t* p){memcpy(nonces,p,16);return 0;}
 int setBufferSession(const uint8_t* p){memcpy(session,p,128);memcpy(&fCntUp,p,4);return 0;}
 uint32_t getLastToA(){return 165;}
 uint8_t getMacUplinkLen(){return 0;}
 virtual int16_t sendReceive(const uint8_t*,size_t,uint8_t,uint8_t*,size_t*,bool,LoRaWANEvent_t* up,LoRaWANEvent_t*){
   assert(nbTrans==1);++physicalSends;lastCounter=fCntUp++;
   if(powerCut)throw std::runtime_error("power lost after RF transmission");
   if(macReply){uint8_t msg[20]={};size_t len=0;macReply=false;this->sendReceive(msg,20,0,nullptr,&len,false,nullptr,nullptr);}
   if(up)up->fCnt=fCntUp;
   return 0;
 }
};
