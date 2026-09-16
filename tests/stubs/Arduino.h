#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cmath>
#include <string>
#include <deque>
#include <stdexcept>
using String=std::string;
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define RTC_DATA_ATTR
extern unsigned long fakeTime;
extern int gpio[49],pinWrites[49];
inline unsigned long millis(){return fakeTime;}
inline void delay(unsigned long ms){fakeTime+=ms;}
inline void pinMode(int p,int){++pinWrites[p];}
inline void digitalWrite(int p,int v){gpio[p]=v;++pinWrites[p];}
inline int digitalRead(int p){return gpio[p];}
struct SerialMock {
 bool connected=true;
 std::deque<char> input;
 std::string output;
 void begin(int){}
 void setRxBufferSize(size_t){}
 void setTxTimeoutMs(unsigned){}
 explicit operator bool()const{return connected;}
 int availableForWrite(){return connected?32:0;}
 size_t write(const uint8_t* p,size_t n){output.append((const char*)p,n);return n;}
 int available(){return input.size();}
 int read(){int c=input.front();input.pop_front();return c;}
 void feed(const std::string& s){for(char c:s) input.push_back(c);}
};
extern SerialMock Serial;
struct ESPMock {
 unsigned long getFreeHeap(){return 200000;}
 void restart(){throw std::runtime_error("restart");}
};
extern ESPMock ESP;
