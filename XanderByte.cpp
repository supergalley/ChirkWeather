#include "XanderByte.h"
static int clampi(int v,int lo,int hi){return v<lo?lo:(v>hi?hi:v);}
static uint8_t encBatt(float v){const float th[7]={3.50,3.62,3.74,3.86,3.98,4.10,4.20};uint8_t b=0;while(b<7&&v>=th[b])b++;return b;}
static uint8_t encHum(float rh){return (uint8_t)clampi((int)lroundf(rh),0,127);}
static uint8_t encPress(float hPa){return (uint8_t)clampi((int)lroundf(hPa)-980,0,63);}
static uint8_t encTemp(float tC){return (uint8_t)clampi((int)lroundf(tC)+20,0,63);}
static uint8_t encWind(float mph){return (uint8_t)clampi((int)lroundf(mph),0,63);}
namespace XanderByte{
  void encode(const XReadings &r,uint8_t out[4]){
    uint8_t W=isfinite(r.windMph)?encWind(r.windMph):0;
    uint8_t D=r.windDirCode&7;
    uint8_t T=isfinite(r.tempC)?encTemp(r.tempC):0;
    uint8_t H=isfinite(r.humidity)?encHum(r.humidity):0;
    uint8_t P=isfinite(r.pressureHpa)?encPress(r.pressureHpa):0;
    uint8_t B=encBatt(r.batteryV);
    uint8_t E=r.sensorError?1:0;
    out[0]=(W&0x3F)|((D&0x03)<<6);
    out[1]=((D>>2)&0x01)|((T&0x3F)<<1)|((H&0x01)<<7);
    out[2]=((H>>1)&0x3F)|((P&0x03)<<6);
    out[3]=((P>>2)&0x0F)|((B&0x07)<<4)|((E&0x01)<<7);
  }
  String hex(const uint8_t *b,uint8_t n){
    char buf[64];int idx=0;
    for(uint8_t i=0;i<n;i++)idx+=sprintf(&buf[idx],"%02X%s",b[i],(i+1<n)?" ":"");
    buf[idx]='\0';
    return String(buf);
  }
}
