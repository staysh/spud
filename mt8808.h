#ifndef MT8808_H
#define MT8808_H

//Uncomment for linker issues
#include "Arduino.h" 

//TODO: implement swap x <=> y could be done with swappin ax/ay pins?
/*
FASTRUN void delayNano(int ns)
{
  const float scaleFactor = 3.0;
  const float offsetFactor = -3.0;
  int iterations = int(ns/scaleFactor + offsetFactor + 0.5);

  for(int i=0; i<iterations; i++)
  {
    __asm__ __volatile__ ("nop\n\t");
  }
}
*/
struct MT8808Pins
{
  uint8_t ax[3];
  uint8_t ay[3];
  uint8_t data;
  uint8_t strobe;
  uint8_t reset;
};
struct MT8808Map
{
  uint8_t x[8];
  uint8_t y[8];
};
class MT8808
{
  public:
    MT8808() = delete;
    MT8808(MT8808Pins p) : pin(p) { };
    void initPins();
    void remap(MT8808Map m) { maps=m; };
    void swapXY() { swapXY_ = swapXY_ ? false : true; }; 
    void reset();
    void tx(uint8_t x, uint8_t y, bool v);
    
  private:
    MT8808Pins pin;
    MT8808Map maps = MT8808Map({ 
      {7,6,5,4,3,2,1,0},
      {0,1,2,3,4,5,6,7} 
    }); //default
    bool swapXY_ = false;
};
#endif

void MT8808::initPins()
{
  for (int i = 0; i < 3; i++)
  {
    pinMode(pin.ax[i], OUTPUT);
    pinMode(pin.ay[i], OUTPUT);
  }
  pinMode(pin.data, OUTPUT);
  pinMode(pin.strobe, OUTPUT);
  pinMode(pin.reset, OUTPUT);
}
void MT8808::reset()
{
  digitalWriteFast(pin.reset, HIGH);
  //delayNano(50);
  delayMicroseconds(1);
  digitalWriteFast(pin.reset, LOW);
}

void MT8808::tx(uint8_t x, uint8_t y, bool v)
{
  for(int i = 0; i < 3; i++)
  {
    digitalWriteFast(pin.ax[i], (maps.x[x] >> i) & 1);
    //Serial.print((maps.x[x] >> i) & 1);
    //Serial.print(" ");
    digitalWriteFast(pin.ay[i], (maps.y[y] >> i) & 1);
    //Serial.print((maps.y[y] >> i) & 1);
    //Serial.print(" ");
    //Serial.println();
  }
  digitalWriteFast(pin.data, v);
  //Serial.print(v);
  //Serial.print("  Data");
  //Serial.println();
  digitalWriteFast(pin.strobe, HIGH);
  //delayNano(50);
  delayMicroseconds(1);
  digitalWriteFast(pin.strobe, LOW);
}
