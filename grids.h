
/* grids : an idiomatic embedded ui library for the monome grid
 * Copyright (c) 2022 staysh (openstarsystems@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef GRIDS_H
#define GRIDS_H

#include "Arduino.h"
#include "USBHost_t36.h"

#define REQUEST_DEVICE_INFO 0x00
#define REQUEST_ID 0x01
#define REQUEST_GRID_SIZE 0x05
#define SET_ONE_ON 0x10
#define SET_ONE_OFF 0x11
#define SET_ALL_ON 0x13
#define SET_ALL_OFF 0x12
#define SET_MAP 0x14        //use d_8 type
#define SET_ROW 0x15
#define SET_COL 0x16
#define SET_LEVEL_MAX 0x17
#define SET_ONE_LEVEL 0x18
#define SET_ALL_LEVEL 0x19
#define SET_MAP_LEVEL 0x1A  //use d_32 type
#define SET_ROW_LEVEL 0x1B  //use d_8 type
#define SET_COL_LEVEL 0x1C  //use d_8 type

typedef void(*CBPtr)(bool, uint8_t, uint8_t);

typedef uint8_t d_8[8];
typedef uint8_t d_32[32];

int8_t toVec(int V)
{
  if(V < -128) return -128;
  if(V > 127) return 127;
  return V;
}

struct Vector
{
  int8_t i;
  int8_t j;
  Vector(int I, int J) : i(toVec(I)), j(toVec(J)) {};
  Vector operator+(const Vector &V) { return Vector( {i+V.i, j+V.j} ); }; 
};

struct Point
{
  uint8_t x:4;
  uint8_t y:4;
  Point(int X, int Y) : x(X), y(Y) {};
};

Point operator+(const Point &P, const Vector &V)
{
  return Point({P.x + V.i, P.y + V.j});
}

Vector operator-(const Point &P1, const Point &P2)
{
  return Vector({P1.x - P2.x, P1.y - P2.y});
}

d_32& mapToD_32 (uint8_t (&m)[64], d_32 &d32)
{
  
  for(int i = 0; i < 64; i += 2)
  {
    d32[i/2] = ( (m[i + 1] << 4) & 0xF0 ) | (m[i] & 0xF);
  }
  return d32;
}

d_8& mapToD_8 (bool (&m)[64], d_8 &d8)
{
  for(int i = 0; i < 8; i++)
  {
    d8[i] = 0;
  }
  for(int i = 0; i < 64; i++)
  {
    d8[i/8] |= (m[i] & 0x1) << (i % 8);
  }
  return d8;
}

class GridBase
{
  public:
    Point offset;
    Vector size;
    GridBase (Point p, Vector s) : offset(p), size(s) { }
};

template<int N>
class Grid : public GridBase
{
  public:
    uint8_t level[N] = { 0 };
    bool key[N] = { 0 };
    Grid (Point p, Vector s) : GridBase(p,s) { }
    typedef uint8_t levels_[N];
    typedef bool keys_[N];
    levels_& levels() { return level; }
    keys_& keys() { return key; }
    uint8_t xy2i (uint8_t x, uint8_t y) { return (x - offset.x) + (y - offset.y) * size.i; }
    Point i2xy (uint8_t i) {return Point( { (i % size.i) + offset.x, (i / size.j) + offset.y} ); }  
};

template<uint8_t gridSize, uint8_t layers>
class Grids
{
  private:
    CBPtr funcTable[gridSize * layers] = { };
    bool active[gridSize * layers] = { };
    uint8_t layer = 0;
    const uint8_t rowSize = gridSize > 64 ? 16 : 8;
    const uint8_t colSize = gridSize > 128 ? 16 : 8;
    USBHost usb;
    USBSerial serial = USBSerial(usb);
  public:
    Grids() {};
    //Grids(uint8_t gs, uint8_t
    void init_()
    {
      usb.begin();
      serial.begin(115200);
    }

    void addGridCallback(GridBase* const g, uint8_t z, CBPtr cb)
    {
      //get b-box
      uint8_t x2, y2;
      x2 = g->offset.x + g->size.i;
      y2 = g->offset.y + g->size.j;
      
      //put CBPtrs in table, turn active to true
      for(uint8_t x = g->offset.x; x < x2; x++)
      {
        for( uint8_t y = g->offset.y; y < y2; y++)
        {
          uint8_t index = x + (y * rowSize) + (z * gridSize);
          funcTable[index] = cb;
          active[index] = true;
        }
      }
    }

    void run()
    {
      while(serial.available())
      {
        uint8_t dest, x, y, index;
        dest = serial.read();
        if ( dest == 0x21 || dest == 0x20)
        {
          x = serial.read();
          y = serial.read();
          index = x + (y * rowSize) + (layer * gridSize);
          if(active[index])
          {
            funcTable[index]( dest & 0b1, x, y);
          }
        }
      }
    }
    
    void write( std::initializer_list<uint8_t> list)
    {
      for( auto data : list)
      {
        serial.write(data);
      }
    }
    
    template<typename T, int N>
    void write( std::initializer_list<uint8_t> list, T (&arr)[N] )
    {
      for( auto data : list)
      {
        serial.write(data); 
      }
      for(auto data : arr)
      {
        serial.write(data);
      }
    }
};
#endif
