#include "sequencer.h"

void Sequencer::next()
{
  switch (mode)
  {
    case TRIG_FWD:
      dir = FORWARD;
      transport.step++;
      if(transport.step > pattern[transport.pattern].seqEnd || transport.step < pattern[transport.pattern].seqStart)
      {
        transport.step = pattern[transport.pattern].seqStart;
        //checkPatternChange();
      }
      break;
    case TRIG_REV:
      dir = REVERSE;
      transport.step--;
      if(transport.step < pattern[transport.pattern].seqStart || transport.step > pattern[transport.pattern].seqEnd) //uint!!
      {
        transport.step = pattern[transport.pattern].seqEnd;
        //checkPatternChange();
      }
      break;
    case TRIG_PNG:
      if(transport.step <= pattern[transport.pattern].seqStart || transport.step >= pattern[transport.pattern].seqEnd)
      {
        dir = !dir;
        if(dir)
          transport.step = pattern[transport.pattern].seqStart;
        else
          transport.step = pattern[transport.pattern].seqEnd;
      }
      if (dir)
        transport.step++;   
      else
        transport.step--;
      break;
    case TRIG_RND:
      transport.step = random(pattern[transport.pattern].seqStart, pattern[transport.pattern].seqEnd + 1);
      break;
    case CV_QUANT:
      transport.step = map(cv->lpf, cv->max, cv->min,
                           pattern[transport.pattern].seqStart, pattern[transport.pattern].seqEnd);
      break;
    default:
      transport.step = transport.step;
      break;
    }

    //if ARRANGER jump what is the play mode of arranger?
    return;
}

void Sequencer::advanceToCue(Cue adv)
{
  transport.pattern = adv.pattern;
  transport.step = adv.step;
  return;
}
void Sequencer::linkCV(CVBuffer * ext)
{
  cv = ext;
  return;
}
