#ifndef CVBUFFER_H
#define CVBUFFER_H

#include "Arduino.h"

//#include <stdint.h>

#define BUFFER_MAX_LEN 32
#define BIT_REDUCE 4
#define MAX_DATA_MASK 0x3F
//8bit cv buffer

struct CVBuffer
{
  int buf[BUFFER_MAX_LEN] = { 0 };
  uint8_t lpf = 0;
  uint8_t count = 0;
  uint8_t min = 0;
  uint8_t max = 63;
  int readPin = 0;
  CVBuffer(uint8_t pin) : readPin(pin) {}
  void setReadPin(uint8_t p)
  {
    readPin = p;
  }
  void average()
  {
    int sum = 0;
    {
      for(uint8_t i = 0; i < BUFFER_MAX_LEN; i++)
      {
        sum += buf[i];
      }
      lpf = sum / BUFFER_MAX_LEN;
    }
  }
  void run()
  {
    shift();
    buf[BUFFER_MAX_LEN - 1] = (analogRead(readPin)) >> 4 & 0x3F;
    //Serial.println(buf[BUFFER_MAX_LEN - 1]);
    if (buf[BUFFER_MAX_LEN - 1] == buf[BUFFER_MAX_LEN - 2])
      count++;
    else
      count = 0;
    average();
  }
  void shift()
  {
    for(uint8_t i = 0; i < BUFFER_MAX_LEN - 1; i++)
    {
      buf[i] = buf[i+1];
    }
  }
};
#endif
