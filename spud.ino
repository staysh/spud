#include "grids.h"
#include "cvbuffer.h"
#include "sequencer.h"
#include "mt8808.h"

#define CLOCKPIN 34
#define CVPIN 31

struct Program {
  bool data[64] = { false };
  //transform algorithms
  //static bool pgmBuffer[64];
  //static
};

Program copyBuffer;
Program transformBuffer;

const uint8_t CV_OVERSAMPLING =  4;
const uint8_t LED_INACTIVE    =  0;
const uint8_t LED_ACTIVE_LOW  =  5;
const uint8_t LED_ACTIVE_MED  =  10;
const uint8_t LED_ACTIVE_HIGH =  15;
const unsigned long TIMER_RES_MICROS = 500;

//ui window callbacks (grids library)
void programEditCB    (bool s, uint8_t x, uint8_t y);
void stepWindowCB     (bool s, uint8_t x, uint8_t y);
void pageWindowCB     (bool s, uint8_t x, uint8_t y);
void patternSelectCB  (bool s, uint8_t x, uint8_t y);
void arrangerCB       (bool s, uint8_t x, uint8_t y);
void leftCtlCB        (bool s, uint8_t x, uint8_t y);
void pgmSelectionCB   (bool s, uint8_t x, uint8_t y);
void pgmTransformCB   (bool s, uint8_t x, uint8_t y);
void rightCtlCB       (bool s, uint8_t x, uint8_t y);
void modeSelectCB     (bool s, uint8_t x, uint8_t y);

uint16_t getProgramIndex(const Sequencer::Cue& cue);
void launchProgram(const Sequencer::Cue& pgm);
void paintProgramWindow (const Sequencer::Cue& pgm);
void paintStepWindowEdit ();
void paintPageWindowEdit ();
void paint3x3Window (const Grid<9>& grid);
void paintRLCtl ();

//temp
void printSelectBounds();

//declare ui zones
Grids<128, 1> grids           =   Grids<128, 1>();
Grid<64>      programEdit     =   Grid<64>({8,0}, {8,8}); 
Grid<8>       stepWindow      =   Grid<8>({0,0}, {8,1});
Grid<8>       pageWindow      =   Grid<8>({0,1}, {8,1});
Grid<8>       patternSelect   =   Grid<8>({0,2}, {8,1});
Grid<8>       arranger        =   Grid<8>({0,3}, {8,1});
Grid<3>       leftCtl         =   Grid<3>({0,4}, {1,3});
Grid<9>       pgmSelection    =   Grid<9>({1,4}, {3,3});
Grid<9>       pgmTransform    =   Grid<9>({4,4}, {3,3});
Grid<3>       rightCtl        =   Grid<3>({7,4}, {1,3});
Grid<8>       modeSelect      =   Grid<8>({0,7}, {8,1});

struct ProgramSelection
{
  uint8_t x1 = 0;
  uint8_t x2 = 1;
  uint8_t y1 = 0;
  uint8_t y2 = 1;
  uint8_t it = 0;
  Program selection;
  ProgramSelection() {update();}
  uint8_t incLeft()  { return x1 = (--x1 > 8) ? ++x1 : x1; }
  uint8_t incRight() { return x2 = (++x2 > 8) ? --x2 : x2; }
  uint8_t incTop()   { return y1 = (--y1 > 8) ? ++y1 : y1; }
  uint8_t incBottom(){ return y2 = (++y2 > 8) ? --y2 : y2; }
  uint8_t decLeft()  { return x1 = (++x1 == x2) ? --x1 : x1; }
  uint8_t decRight() { return x2 = (--x2 == x1) ? ++x2 : x2; }
  uint8_t decTop()   { return y1 = (++y1 == y2) ? --y1 : y1; }
  uint8_t decBottom(){ return y2 = (--y2 == y1) ? ++y2 : y2; }
  //try again if speed is an issue
  //void rowOn(uint8_t row)   { for(it = x1; it < x2; it++) selection.data[it + row * 8] = true;} 
  //void rowOff(uint8_t row)  { for(it = x1; it < x2; it++) selection.data[it + row * 8] = false;}
  //void colOn(uint8_t col)   { for(it = y1; it < y2; it++) selection.data[col + it * 8] = true;}
  //void colOff(uint8_t col)  { for(it = y1; it < y2; it++) selection.data[col + it * 8] = false;}
  void update()
  {
    for(uint8_t i = 0; i < 8; i++)
    {
      for(uint8_t j = 0; j < 8; j++)
      {
        if(j >= x1 && j < x2 && i >= y1 && i < y2)
        {
          selection.data[j + i * 8] = true;
        }
        else
        {
          selection.data[j + i * 8] = false;
        }
      }
    }
  }
  /****
   * Usage: 
   * uint8_t temp = x1;
   * if(temp != incLeft()) colOn(x1);
   * 
   * uint8_t temp = y2;
   * if(temp != decBottom()) row0ff(y2); 
   */
} pgmSelect;

enum SelectMode
{
  INCR,
  DECR,
  MOVE
} pgmSelectMode;

uint8_t decrPad = 0;
uint8_t incrPad = 2;

//MT8808 
MT8808 switches = MT8808
({
  {18, 19, 20},     //ax pins
  {15, 16, 17},     //ay pins
  22,               //data pin
  21,               //strobe pin
  23                //reset pin
});

//hardware
const uint8_t clockPin = CLOCKPIN;
const uint8_t cvPin = CVPIN;
const uint8_t dac = A21;
CVBuffer cv{cvPin};

//engine & data
Sequencer sequencer;
Sequencer::Cue edit;
Program pgmData[512];
d_8 d8Buffer = { 0 }; 
d_32 d32Buffer = { 0 };
uint8_t transportPage = 0;
uint8_t editPage = 0;
uint8_t editPageCursor = 0;
bool selectionDisplay = true;
uint8_t tempStart = 0;
uint8_t tempEnd = 0;
//bool lengthListener = false;
//bool patternStartSelected = false;
//bool patternTouch = false;
bool watchClock = LOW; 
unsigned long lastClockRead = 0;

//state and settings (future struct?)
struct SpudState {
  bool observe = false;
  bool safeMode = true;
  bool seqActive = false;
  //bool windowFlags[numWindows] <-TODO
  bool scheduledPatternChange = false;
  bool seqStartListener = false;
  bool seqEndListener = false;
  SpudState() {} 
} state{};

void _init();
void _run();

void setup ()
{
  _init();
}
void loop ()
{
  _run();
}
void _init()
{
  //attach callbacks to lookuptable
  grids.init_();
  grids.addGridCallback (&programEdit,    0, programEditCB);
  grids.addGridCallback (&stepWindow,     0, stepWindowCB);
  grids.addGridCallback (&pageWindow,     0, pageWindowCB);
  grids.addGridCallback (&patternSelect,  0, patternSelectCB);
  grids.addGridCallback (&arranger,       0, arrangerCB);
  grids.addGridCallback (&leftCtl,        0, leftCtlCB);
  grids.addGridCallback (&pgmSelection,   0, pgmSelectionCB);
  grids.addGridCallback (&pgmTransform,   0, pgmTransformCB);
  grids.addGridCallback (&rightCtl,       0, rightCtlCB);
  grids.addGridCallback (&modeSelect,     0, modeSelectCB);

  //set up matrix driver
  switches.initPins();
  switches.reset();

  //init teensy peripherals
  pinMode(clockPin, INPUT);
  //pinMode(cvPin, INPUT);
  //pinMode(dac, OUTPUT);
  analogWrite(dac, 1023);
  sequencer.linkCV(&cv);
  sequencer.mode = Sequencer::MANUAL;
  
  //draw welcome screen

  //draw ui
  //set up cursor windows init values
  for(uint8_t i = 0; i < 9; i++)
  {
    if(i % 2 == 1)
    {
      pgmSelection.level[i] = LED_ACTIVE_MED;
      pgmTransform.level[i] = LED_ACTIVE_MED;
    }
    else
    {
      pgmSelection.level[i] = LED_ACTIVE_LOW;
      pgmTransform.level[i] = LED_ACTIVE_LOW;
    }
  }
  //INCR MODE DEFAULT
  pgmSelection.level[incrPad] = LED_ACTIVE_HIGH;
  
  for(uint8_t i = 0; i < 3; i++)
  {
    leftCtl.level[i] = LED_ACTIVE_LOW;
    rightCtl.level[i] = LED_ACTIVE_LOW;
  }

  //for(uint8_t i = 0; i < 8; i++) pgmSelect.selection.data[i] = true;

  //initial paints
  paintStepWindowEdit();
  paintPageWindowEdit();
  paintPatternSelect();
  paintArranger();
  paintModeSelect();
  paint3x3Window(pgmSelection);
  paint3x3Window(pgmTransform);
  paintRLCtl();
  paintProgramWindow(edit);
}

void _run()
{
  grids.run();
  cv.run();
  //TIMER_RES sets max rate to drive sequencer
  //500 micros can catch up to 1000Hz square wave
  unsigned long t = micros();
  if(t - lastClockRead > TIMER_RES_MICROS)
  {
    lastClockRead = t;
    bool newClockRead = digitalReadFast(clockPin);
    if(newClockRead && !watchClock)
    {
      //pulse rising edge
      //if(sequencer.mode != Sequencer::MANUAL && sequencer.mode != Sequencer::CV_FREE)
      switch(sequencer.mode)
      {
        case Sequencer::MANUAL:
          break;
        case Sequencer::CV_FREE:
          sequencer.transport.step = map(cv.lpf, cv.max, cv.min,
                                       sequencer.pattern[sequencer.transport.pattern].seqStart,
                                       sequencer.pattern[sequencer.transport.pattern].seqEnd);
          launchProgram(sequencer.transport);
          break;
        default:
          sequencer.next();
          launchProgram(sequencer.transport);
          //Serial.println("shouldnt be here");
          break;
      }
    }
    watchClock = newClockRead;
  }
}

void programEditCB(bool s, uint8_t x, uint8_t y)
{
  if(!state.observe)
  {
    if(s)
    {
      uint8_t index = programEdit.xy2i(x, y);
      uint8_t xOff = x - programEdit.offset.x;
      programEdit.key[index] = pgmData[edit.step + edit.pattern * NUM_MAX_STEPS].data[index]
                             = pgmData[edit.step + edit.pattern * NUM_MAX_STEPS].data[index] ? false : true;
      
      if(programEdit.key[index] && state.safeMode)
      {
        for(uint8_t i = 0; i < programEdit.size.j; i++)
        {
          uint8_t colIndex = xOff + i * programEdit.size.j;
          if(colIndex != index && programEdit.key[colIndex])
          {
            programEdit.key[colIndex] = pgmData[edit.step + edit.pattern * NUM_MAX_STEPS].data[colIndex] = false;
            switches.tx(xOff, i, false);
            //grids.write( { 0x10, x, i} );
          }
        }
      }
      switches.tx(xOff, y, programEdit.key[index]);
      //grids.write( { (programEdit.key[index] ? 0x11 : 0x10), x, y } );
      paintProgramWindow(edit);
    }
  }
  return;
}

void stepWindowCB(bool s, uint8_t x, uint8_t y)
{
  if(s)
  {
    bool droppedMode = false;
    
    if(state.observe)
    {
      state.observe = false;
      edit = sequencer.transport;
      droppedMode = true;
    }
    
    if(state.seqStartListener)
    {
      uint8_t temp = editPageCursor * stepWindow.size.i + x;
      if(temp < sequencer.pattern[edit.pattern].seqEnd)
        sequencer.pattern[edit.pattern].seqStart = temp;
        //Serial.println(sequencer.pattern[edit.pattern].seqStart);
    }
    else if(state.seqEndListener)
    {
      uint8_t temp = editPageCursor * stepWindow.size.i + x;
      if(temp > sequencer.pattern[edit.pattern].seqStart)
        sequencer.pattern[edit.pattern].seqEnd = temp;
    }
    else
    {
      edit.step = (editPageCursor * stepWindow.size.i) + x;
      paintProgramWindow(edit);
      if(droppedMode)
      {
        paintPageWindowEdit();
      }
      if(sequencer.mode == Sequencer::MANUAL)
      {
        launchProgram(edit);
      }
    }
    paintStepWindowEdit(); 
  }
  return;
}

void pageWindowCB(bool s, uint8_t x, uint8_t y)
{
  if(s)
  {
    bool droppedMode = false;
    if(state.observe)
    {
      state.observe = false;
      edit = sequencer.transport;
      droppedMode = true;
    }
    
    editPageCursor = x + pageWindow.offset.x;
    paintPageWindowEdit();
    paintStepWindowEdit();
  }
  return;
}

void patternSelectCB  (bool s, uint8_t x, uint8_t y)
{
  
  if(s)
  {
    uint8_t selected = x - patternSelect.offset.x;
    if(edit.pattern != selected)
    {
      edit.pattern = selected;
      edit.step = sequencer.pattern[edit.pattern].seqStart;
      editPageCursor = 0;
      paintPatternSelect();
      paintStepWindowEdit();
      paintPageWindowEdit();
      //Serial.println(sequencer.transport.pattern);
    }
  }
}

void arrangerCB       (bool s, uint8_t x, uint8_t y)
{
  if(s)
  {
    uint8_t selected = x - arranger.offset.x;
    sequencer.arranger.isSet[selected] = !sequencer.arranger.isSet[selected];
    sequencer.arranger.jump[selected] = Sequencer::Cue({sequencer.pattern[edit.pattern].seqStart, edit.pattern});
    paintArranger();
  }
}
void leftCtlCB        (bool s, uint8_t x, uint8_t y)
{
  if(s)
  {
    uint8_t index = leftCtl.xy2i(x, y);
    if(index == 0)
    {
      state.observe = !state.observe;
      if(state.observe)
        grids.write({SET_ONE_LEVEL, x, y, LED_ACTIVE_HIGH});
      else
        grids.write({SET_ONE_LEVEL, x, y, LED_ACTIVE_LOW});      
    }
  }
}
void pgmSelectionCB   (bool s, uint8_t x, uint8_t y)
{
  uint8_t temp = 0;
  //Serial.println(temp);
  if(s)
  {
    uint8_t pad = pgmSelection.xy2i(x,y);
    //Serial.println(pad);
    switch(pad)
    {
      case 0:
        pgmSelectMode = DECR;
        pgmSelection.level[decrPad] = LED_ACTIVE_HIGH;
        pgmSelection.level[incrPad] = LED_ACTIVE_LOW;
        break;
      case 1:
        if(pgmSelectMode == DECR)
        {
          temp = pgmSelect.y2;
          if(temp != pgmSelect.decBottom()) pgmSelect.update();
        }
        else if(pgmSelectMode == INCR)
        {
          temp = pgmSelect.y1;
          if(temp != pgmSelect.incTop()) pgmSelect.update();
        }
        else
        {
          //move;
        }
        break;
      case 2:
        pgmSelectMode = INCR;
        pgmSelection.level[incrPad] = LED_ACTIVE_HIGH;
        pgmSelection.level[decrPad] = LED_ACTIVE_LOW;
        break;
      case 3:
        if(pgmSelectMode == DECR)
        {
          temp = pgmSelect.x2;
          if(temp != pgmSelect.decRight()) pgmSelect.update();
        }
        else if(pgmSelectMode == INCR)
        {
          temp = pgmSelect.x1;
          if(temp != pgmSelect.incLeft()) pgmSelect.update();
        }
        else
        {
          //move;
        }
        break;
      case 4:
        selectionDisplay = selectionDisplay ? false : true;
        break;
      case 5:
        if(pgmSelectMode == DECR)
        {
          temp = pgmSelect.x1;
          if(temp != pgmSelect.decLeft()) pgmSelect.update();
        }
        else if(pgmSelectMode == INCR)
        {
          temp = pgmSelect.x2;
          if(temp != pgmSelect.incRight()) pgmSelect.update();
        }
        else
        {
          //move;
        }
        break;
      case 6:
        //copy
        break;
      case 7:
        if(pgmSelectMode == DECR)
        {
          temp = pgmSelect.y1;
          if(temp != pgmSelect.decTop()) pgmSelect.update();
        }
        else if(pgmSelectMode == INCR)
        {
          temp = pgmSelect.y2;
          if(temp != pgmSelect.incBottom()) pgmSelect.update();
        }
        else
        {
          //move;
        }
      case 8:
        //paste
        break;
      default:
        break;       
    }
    paint3x3Window(pgmSelection);
    //printSelectBounds();
    paintProgramWindow(edit); //can we optimize?
  }
}
void pgmTransformCB   (bool s, uint8_t x, uint8_t y)
{
  
}
void rightCtlCB       (bool s, uint8_t x, uint8_t y)
{
  uint8_t index = rightCtl.xy2i(x,y);
  if(s)
  {
    if(index == 0)
    {
      state.seqStartListener = true;
      //Serial.println("true");
    }
    else if(index == 1)
    {
      state.seqEndListener = true;
    }
    else
    {
      //dac
    }
  }
  else
  {
    if(index == 0)
    {
      state.seqStartListener = false;
      paintStepWindowEdit();
      paintPageWindowEdit();
      paintPatternSelect();
     // Serial.println("false");
    }
    else if(index == 1)
    {
      state.seqEndListener = false;
      paintStepWindowEdit();
      paintPageWindowEdit();
      paintPatternSelect();
    }
    else
    {
      //dac
    }    
  }
}
void modeSelectCB(bool s, uint8_t x, uint8_t y)
{
  Sequencer::Mode m = Sequencer::Mode(x); 
  if(sequencer.mode != m) 
  {
    if(s)
    {
      //grids.write({SET_ONE_LEVEL, static_cast<uint8_t>(sequencer.mode), modeSelect.offset.y, LED_ACTIVE_LOW});
      sequencer.mode = m;
      //grids.write({SET_ONE_LEVEL, x + modeSelect.offset.x, modeSelect.offset.y, LED_ACTIVE_HIGH});
      if(sequencer.mode == Sequencer::MANUAL)
      {
        edit = sequencer.transport;
        editPageCursor = edit.step / stepWindow.size.i;
        launchProgram(edit); //might not need
        paintStepWindowEdit();
        paintPageWindowEdit();
      }
      paintModeSelect();
    }
  }
}

void paintProgramWindow (const Sequencer::Cue& pgm)
{
  //create a d32 of pgmSelection and pgmState
  d_32 tx = { 0 };
  uint8_t index = pgm.step + pgm.pattern * NUM_MAX_STEPS;
  for(uint8_t i = 0; i < 64; i++)
  {
    //tx[i] = 0; 
    uint8_t led = LED_INACTIVE;
    if(pgmSelect.selection.data[i]) led = LED_ACTIVE_LOW;
    if(pgmData[index].data[i]) led = LED_ACTIVE_HIGH;
    tx[i/2] |= (led & 0xF) << (4 * ((i + 1) % 2));
  }
  
  grids.write( {SET_MAP_LEVEL, programEdit.offset.x, programEdit.offset.y}, tx);
}

void paintStepWindowEdit ()
{
  uint8_t pageOffset = stepWindow.size.i * editPageCursor;
  uint8_t length = 1 + sequencer.pattern[edit.pattern].seqEnd - sequencer.pattern[edit.pattern].seqStart;
  uint8_t start = sequencer.pattern[edit.pattern].seqStart;

  d_8 data = { 0 };

  for(uint8_t i = 0; i < stepWindow.size.i; i++)
  {
    //data[i] = 0;
    uint8_t led = LED_INACTIVE;
    uint8_t index = i + pageOffset;
    uint8_t test = index - start;
    if(test < length ) led = LED_ACTIVE_LOW;
    if(index == sequencer.transport.step && edit.pattern == sequencer.transport.pattern)
      led = LED_ACTIVE_MED;
    if(index == edit.step) led = LED_ACTIVE_HIGH;
    data[i/2] |= (led & 0xF) << (4 * ((i + 1) % 2));
  }
  grids.write({SET_ROW_LEVEL, stepWindow.offset.x, stepWindow.offset.y}, data);
}

void paintPageWindowEdit ()
{
  uint8_t start = sequencer.pattern[edit.pattern].seqStart / pageWindow.size.i;
  uint8_t length = sequencer.pattern[edit.pattern].seqEnd / pageWindow.size.i - start;
  uint8_t transportPage = sequencer.transport.step / pageWindow.size.i;

  d_8 data = { 0 };
  
  for(uint8_t i = 0; i < pageWindow.size.i; i++)
  {
    //data[i] = 0;
    uint8_t led = LED_INACTIVE;
    uint8_t test = i - start;
    if(test <= length) led = LED_ACTIVE_LOW;
    if(i == transportPage && edit.pattern == sequencer.transport.pattern)
      led = LED_ACTIVE_MED;
    if(i == editPageCursor) led = LED_ACTIVE_HIGH;
    data[i/2] |= (led & 0xF) << (4 * ((i + 1) % 2));
  }
  grids.write({SET_ROW_LEVEL, pageWindow.offset.x, pageWindow.offset.y}, data);
}

void paintPatternSelect ()
{
  d_8 data = { 0 };
  
  for(uint8_t i = 0; i < patternSelect.size.i; i++)
  {
    //data[i] = 0;
    uint8_t led = LED_ACTIVE_LOW;
    if(sequencer.transport.pattern == i) led = LED_ACTIVE_MED;
    if(edit.pattern == i) led = LED_ACTIVE_HIGH;
    data[i/2] |= (led & 0xF) << (4 * ((i + 1) % 2));
  }
  grids.write({SET_ROW_LEVEL, patternSelect.offset.x, patternSelect.offset.y}, data);
}

void paintArranger ()
{
  d_8 data;
  
  for(uint8_t i = 0; i < arranger.size.i; i++)
  {
    data[i] = 0;
    uint8_t led = LED_ACTIVE_LOW;
    if(sequencer.arranger.isSet[i]) led = LED_ACTIVE_MED;
    if(sequencer.mode == Sequencer::ARRANGE && sequencer.arranger.section == i) led = LED_ACTIVE_HIGH;
    data[i/2] |= (led & 0xF) << (4 * ((i + 1) % 2));
  }
  grids.write({SET_ROW_LEVEL, arranger.offset.x, arranger.offset.y}, data);
}

void paintModeSelect ()
{
  d_8 data;
  
  for(uint8_t i = 0; i < modeSelect.size.i; i++)
  {
    data[i] = 0;
    uint8_t led = LED_ACTIVE_LOW;
    if(static_cast<uint8_t>(sequencer.mode) == i) led = LED_ACTIVE_MED;
    data[i/2] |= (led & 0xF) << (4 * ((i + 1) % 2));
  }
  grids.write({SET_ROW_LEVEL, modeSelect.offset.x, modeSelect.offset.y}, data);
}

void launchProgram(const Sequencer::Cue& pgm)
{
  //set switches on MT8088
  for(int i = 0; i < 8; i++)
  {
    for(int j = 0; j < 8; j++)
    {
      switches.tx(i, j, pgmData[getProgramIndex(pgm)].data[i + (j * 8)] );
    }
  }
  //if(sequencer.transport != pgm) sequencer.advanceToCue(pgm);
  //transportPage = sequencer.transport.step / pageWindow.size.i;

  if(sequencer.mode == Sequencer::MANUAL) sequencer.transport = edit;
  
  if(state.observe)
  {
    edit = sequencer.transport;
    editPageCursor = edit.step / pageWindow.size.i;
    paintProgramWindow(sequencer.transport);
  }  
  paintStepWindowEdit();
  paintPageWindowEdit();
  paintPatternSelect();

}

void paint3x3Window (const Grid<9>& grid)
{
  for(uint8_t i = 0; i < 9; i++)
  {
    grids.write({SET_ONE_LEVEL, i % grid.size.i + grid.offset.x, i / grid.size.i + grid.offset.y, grid.level[i]});
  }
}

void paintRLCtl ()
{
  for(uint8_t i = 0; i < 3; i++)
  {
    grids.write({SET_ONE_LEVEL, leftCtl.offset.x, i + leftCtl.offset.y, leftCtl.level[i]});
    grids.write({SET_ONE_LEVEL, rightCtl.offset.x, i + rightCtl.offset.y, rightCtl.level[i]});    
  }
}

uint16_t getProgramIndex (const Sequencer::Cue& cue)
{
  return cue.step + cue.pattern * NUM_MAX_STEPS;
}
void printSelectBounds()
{
  
  Serial.print("x1: ");
  Serial.print(pgmSelect.x1);
  Serial.println();
  Serial.print("x2: ");
  Serial.print(pgmSelect.x2);
  Serial.println();
  Serial.print("y1: ");
  Serial.print(pgmSelect.y1);
  Serial.println();
  Serial.print("y2: ");
  Serial.print(pgmSelect.y2);
  Serial.println();
  /*
  for(uint8_t i = 0; i < 8; i++)
  {
    
    for (uint8_t j = 0; j < 8; j++)
    {
      Serial.print(pgmSelect.selection.data[j + i * 8]);
    }
    Serial.println();
  }
  */
}
