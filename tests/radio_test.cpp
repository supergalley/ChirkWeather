#include <cassert>
#include <iostream>
#include "../XRadio.cpp"
unsigned long fakeTime=0;
int gpio[49]={},pinWrites[49]={};
SerialMock Serial;ESPMock ESP;
namespace XUsb{void log(const char*,...) {}}
void reboot(){isReady=false;prefsOpen=false;storageOK=true;sentThisBoot=false;node=StationNode(&radio,&EU868,1);fakeTime=0;}
int main(){
 uint8_t payload[4]={};
 // Unknown identity/state fails closed.
 assert(!XRadio::begin());assert(physicalSends==0);
 uint32_t legacy=14;prefs.putBytes("fcntup",&legacy,4);reboot();
 assert(XRadio::begin());assert(node.nextCounter()==46);
 // A power cut after RF but before saving the result cannot reuse that counter.
 powerCut=true;
 try {XRadio::send(payload,4);assert(false);}catch(const std::runtime_error&){}
 assert(lastCounter==46);powerCut=false;reboot();
 assert(XRadio::begin());assert(node.nextCounter()==47);
 assert(XRadio::send(payload,4)==0 && lastCounter==47);
 int sent=physicalSends;assert(XRadio::send(payload,4)<0 && physicalSends==sent);
 // MAC-only replies are deferred, including over reset, rather than immediately sent.
 fakeTime+=600000;macReply=true;
 assert(XRadio::send(payload,4)==0 && physicalSends==sent+1 && node.pendingLength==20);
 reboot();assert(XRadio::begin() && node.pendingLength==20);
 assert(XRadio::send(payload,4)==0 && node.pendingLength==0);
 assert(XRadio::sleepSeconds()>600); // Larger frames lengthen the sleep to preserve airtime.
 // Refuse RF when reserving a counter cannot be committed.
 fakeTime+=86400000;storageFail=true;sent=physicalSends;
 assert(XRadio::send(payload,4)<0 && physicalSends==sent);
 storageFail=false;stored["state_v2"].pop_back();reboot();
 assert(!XRadio::begin()); // Corrupted record never silently falls back to zero.
 std::cout<<"Radio persistence, interrupted save, cooldown, deferred MAC and storage failure tests passed\n";
}
