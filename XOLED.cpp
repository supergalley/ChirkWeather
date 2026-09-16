#include "XOLED.h"
#include <U8g2lib.h>
#include <Wire.h>
#include "XPins.h"
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0,PIN_OLED_RST,PIN_OLED_SCL,PIN_OLED_SDA);
static bool oledOn=false;
static bool displayStarted=false;
static bool blanked=false;
static char lines[6][22]={"","","","","",""};
static const uint8_t lineY[6]={10,20,30,40,50,60};
static void copyLine(uint8_t line,const char *text){
  if(line>5) return;
  if(text==nullptr) text="";
  strncpy(lines[line],text,21);
  lines[line][21]='\0';
}
namespace XOLED{
  void clearLine(uint8_t line){
    if(line>5) return;
    lines[line][0]='\0';
    if(oledOn) redraw();
  }
  void clearScreen(){
    for(uint8_t i=0;i<6;i++) lines[i][0]='\0';
    if(oledOn) redraw();
  }
  void displayLine(uint8_t line,const char *text){
    copyLine(line,text);
    if(oledOn) redraw();
  }
  void displayLine(uint8_t line,const String &text){
    displayLine(line,text.c_str());
  }
  bool isOn(){
    return oledOn;
  }
  void powerDown(){
    if(!oledOn) return;
    display.clearBuffer();
    display.sendBuffer();
    digitalWrite(PIN_VEXT,HIGH);
    oledOn=false;
  }
  void powerUp(){
    if(oledOn) return;
    pinMode(PIN_VEXT,OUTPUT);
    pinMode(PIN_OLED_RST,OUTPUT);
    digitalWrite(PIN_VEXT,LOW);
    delay(100);
    digitalWrite(PIN_OLED_RST,LOW);
    delay(20);
    digitalWrite(PIN_OLED_RST,HIGH);
    delay(100);
    Wire.begin(PIN_OLED_SDA,PIN_OLED_SCL);
    Wire.setTimeOut(50);
    display.begin();
    blanked=false;
    display.setFont(u8g2_font_6x10_tf);
    displayStarted=true;
    oledOn=true;
    redraw();
  }
  void blank(bool value){
    if(!oledOn) return;
    blanked=value;
    display.setPowerSave(value?1:0); // Keep Vext stable for the whole diagnostic session.
    if(!value) redraw();
  }
  void redraw(){
    if(!oledOn || blanked) return;
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    for(uint8_t i=0;i<6;i++){
      if(lines[i][0]!='\0') display.drawStr(0,lineY[i],lines[i]);
    }
    display.sendBuffer();
  }
  void solidLine(uint8_t line){
    if(!oledOn) return;
    if(line>5) return;
    display.setDrawColor(1);
    display.drawBox(0,lineY[line]-8,128,10);
    display.sendBuffer();
  }
}
