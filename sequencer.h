#ifndef SEQUENCER_H
#define SEQUENCER_H

//#include <Arduino.h>
#include "cvbuffer.h"

#define FORWARD true
#define REVERSE false
#define NUM_MAX_STEPS 64
#define PATTERNS 8

class Sequencer
{
  public:
    enum Mode
    {
      MANUAL,
      TRIG_FWD,
      TRIG_REV,
      TRIG_PNG,
      TRIG_RND,
      CV_FREE,
      CV_QUANT,
      ARRANGE
    } mode = MANUAL;
    struct Cue
    {
      uint8_t step = 0;
      uint8_t pattern = 0;
      Cue() {}
      Cue(uint8_t s, uint8_t p) : step(s), pattern(p) {} 
      bool operator!=(const Cue& src) { return (src.step == step && src.pattern == pattern); }
    } transport;
    Sequencer() {}
    ~Sequencer() { cv = nullptr; }
    void next(void);
    void advanceToCue(Cue adv);
    void linkCV(CVBuffer * ext);
    struct Arranger
    {
      Cue jump[8];
      bool isSet[8] = { false };
      uint8_t section = 0;
    } arranger;
    struct Pattern
    {
      uint8_t step = 0;
      uint8_t numSteps = 64;
      uint8_t seqStart = 0;
      uint8_t seqEnd = 63;
      uint8_t maxStep = NUM_MAX_STEPS;
    } pattern[PATTERNS];
    bool dir = FORWARD;
    CVBuffer * cv = nullptr;
};
#endif

// "BANK" array of Sequencers
// "Arranger" array of Pointers to Banks
