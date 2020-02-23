/* Rev: 2020-01-29 1st attempt at porting Sketch to STM32 'Blue Pill' board*/
/*
     The TFTLCD library comes from here:  https://github.com/prenticedavid/MCUFRIEND_kbv
      note: after inporting the above library using the ide add zip library menu option, remane the folder to remover the word "master"
  ^   The Touch Screen & GFX libraries are standard (unmodified) Adafruit libraries & were found here:4
         https://github.com/adafruit/Adafruit-GFX-Library
         https://github.com/adafruit/Touch-Screen-Library
*/
char RevDate[9] = "20200218";
// MCU Friend TFT Display to STM32F (Blue Pill) pin connections
//LCD pins  |D7 |D6 |D5 |D4 |D3 |D2 |D1 |D0 | |RD |WR |RS |CS |RST|
//STM32 pin |PA7|PA6|PA5|PA4|PA3|PA2|PA1|PA0| |PB0|PB6|PB7|PB8|PB9|

//#include <TouchScreen.h>
#include "TouchScreen_kbv.h" //hacked version by david prentice

#include <Adafruit_GFX.h>
#include <gfxfont.h>
#include <MCUFRIEND_kbv.h>
MCUFRIEND_kbv tft;
/*
  Rev: 2020-01-27
   a Goertzel Algorythem Tone Detection process
   this time using a more balanced sampling interval
   along with a more simplified tone/squelch algorythem
   plus finding a optimized sampling rate.
   This version seeks to detect and indicate tone frequency hi or low
   output via the RGB LED

*/

/*
   This Sketch was originally written to run on an SS Micro.
   but here is modified to run on the SMT32 Blue Pill Board
   Its purpose is to use the SMT32 Blue Pill Board as a audio tone detector for CW signals
   the digital output of this sketch (SS Micro Digital Pin 11) could act as an the input
   to drive the STM32 CW decoder sketch

*/

/*
   See comment linked to the "SlowData" variable for fast CW transmissions
*/

/*
   This version includes ASD1293 RGB LED Tuning indicator
   Ideally when the LED is Green, the pass band is "centered" on the CW tone
   Conversely when the LED is Blue the CW tone is above the "Pass Band"
   or if the LED is Red the tone is below the "Pass Band"
   As wired in this version of the sketch. the ADS1293 input pin is connected to the Micro's Digital pin 14.
   Note to use this LED you will need to include the Adafruit_NeoPixel library. Intrustions for installing it can be found at
   https://learn.adafruit.com/adafruit-neopixel-uberguide/arduino-library-installation
*/
/*
   In its current form, this sketch is setup to caputure Audio tones @ ~750Hz.
   If a another freq is perferred, change the value assigned to the "TARGET_FREQUENCYC" variable.
*/

//#define DataOutPin PB5 //Sound process (Tone Detector) output pin; i.e. Key signal
#define DataOutPin PA9 //Sound process (Tone Detector) output pin; i.e. Key signal
// #define SlowData 6 // Manually Ground this Digital pin when when the incoming CW transmission is >30WPM
// When this pin is left open, slower code transmissions should work, & noise immunity will be better

// Timer2 interrupt stuff
#define SAMPL_RATE 94    // in microseconds; should give an interrupt rate of 10,526Hz (95)
#define LED_BUILTIN PC13
bool TonPltFlg =  true;// false;//used to control Arduino plotting (via) the USB serial port; To enable, don't modify here, but modify in the sketch
bool Test = false;//  true;//  //Controls Serial port printing in the decode character process; To Enable/Disable, modify here
/*
   Goertzel stuff
*/
#define FLOATING float

#define TARGET_FREQUENCYC  749.5 //Hz
#define TARGET_FREQUENCYL  734.0 //Hz
#define TARGET_FREQUENCYH  766.0 //Hz
#define CYCLE_CNT 6.0 // Number of Cycles to Sample per loop
float SAMPLING_RATE = 10850.0;// 10750.0;// 10520.0;//10126.5 ;//10520.0;//21900.0;//22000.0;//22800.0;//21750.0;//21990.0;
int CurCnt = 0;
float AvgVal = 0.0;
bool OvrLd = false;

int N = 0; //Block size sample 6 cylces of a 750Hz tone; ~8ms interval
int NL = 0;
int NC = 0;
int NH = 0;

FLOATING Q1;
FLOATING Q2;
FLOATING Q1H;
FLOATING Q2H;
FLOATING Q1C;
FLOATING Q2C;

FLOATING cosine;

FLOATING coeff;
FLOATING coeffL;
FLOATING coeffC;
FLOATING coeffH;

float mag = 0;
float magC = 0;
float magC2 = 0;
float magL = 0;
float magH = 0;
int kOld = 0;
// End Goertzel stuff

float ToneLvl = 0;
float noise = 0;
float AvgToneSqlch = 0;
int ToneOnCnt = 0;
int KeyState = 0;
int ltrCmplt = -1500;

/*
   Note. Library uses SPI1
   Connect the WS2812B data input to MOSI on your board.

*/

#include "WS2812B.h"
#define NUM_LEDS 1
WS2812B strip = WS2812B(NUM_LEDS);



byte LEDRED = 0;
byte LEDGREEN = 0;
byte LEDBLUE = 0;
//byte LEDWHITE =80; //Sets Brightness of  Down White Light
byte LightLvl = 0;

long NoiseAvgLvl = 40;
float MidPt = 0;
float SqlchVal = 0.0;
int loopcnt = 4;
byte delayLine = 0;
byte delayLine2 = 0;
bool armHi = false;
bool armLo = false;
bool toneDetect = false;
/////////////////////////////////////////////////////////////////////
// CW Decoder Global Variables

//MCUfriends.com touch screen corner values
const int TS_LEFT=939,TS_RT=97,TS_TOP=891,TS_BOT=121; //values found using "TouchScreen_Calibr_native; Because this sketch runs in porteait mode, the Calibr values require translation

//MCUfriend.com touch screen pin assignments
/*
    Touchscreen needs XM, YP to be on Analog capable pins.  Measure resistance with DMM to determine X, Y
    300R pair is XP, XM.  500R pair is YP, YM.  choose XM, YP from PA7, PA6.  XP, YM from PB6, PB7
    Run the Calibration sketch to get accurate TS_LEFT, TS_RT, TS_TOP, TS_BOT values.
    Ignore the XP, XM, ... values.   They mean nothing on a BluePill

    Adafruit_Touchscreen might need: typedef volatile uint32_t RwReg; JMH 20200126 modified Adafruit TouchScreen.h, found in the libraries folder, by adding the suggested code/declaration

    Maple core:  use Touchscreen_kbv library
    STM Core:    regular Touchscreen libraries should be ok.
*/
const int XP = PB6, YP = PA6, XM = PA7, YM = PB7;
TouchScreen_kbv ts(XP, YP, XM, YM, 300);
TSPoint_kbv tp; //global point
int tchcnt = 25;//initial block Display touch screen Scan loop cnt; actual loop cnt is set in the sketch


//#define MINPRESSURE 10
//#define MAXPRESSURE 1000

#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF



//#define interruptPin PB4 //CW decoder input pin
#define interruptPin PA8 //CW decoder input pin
boolean BlkIntrpt = false;// prevent analog touch screen scan from interfering with analog tone sampling 
boolean buttonEnabled = true;
uint16_t ID;
unsigned long bfrchksum;// used to compare/check 'speed' message strings
int cnt = 0; //used in scrollpage routine
int btnPrsdCnt = 0;
int NoTchCnt = 0;
int TEcnt = 0;
int textSrtX = 0;
int textSrtY = 0;
int scrnHeight = 0;
int scrnWidth;
int px = 0; //mapped 'x' display touch value
int py = 0; //mapped 'y' display touch value
int LineLen = 27; //max number of characters to be displayed on one line
//int row = 7;//7; //max number of lines to be displayed
//int ylo = 200; //button touch reading 2.8" screen
//int yhi = 250; //button touch reading 2.8" screen
//int Stxl = 21; //Status touch reading 2.8" screen
//int Stxh = 130; //Status touch reading 2.8" screen
//int bxlo = 130; //blue button (CLEAR) touch reading 2.8" screen
//int bxhi = 200; //blue button (CLEAR)touch reading 2.8" screen
//int gxlo  = 200; //green button(MODE) touch reading 2.8" screen
//int gxhi  = 290; //green touch (MODE)reading 2.5" screen
//int CPL = 27; //number of characters each line that a 2.8" screen can contain 

int row = 10; //max number of decoded text rows that can be displayed on a 3.5" screen
int ylo = 0; //button touch reading 3.5" screen
int yhi = 40; //button touch reading 3.5" screen
int Stxl = 0; //Status touch reading 3.5" screen
int Stxh = 150; //Status touch reading 3.5" screen
int bxlo = 155; //button touch reading 3.5" screen
int bxhi = 236; //button touch reading 3.5" screen
int gxlo  = 238; //button touch reading 3.5" screen
int gxhi  = 325; //button touch reading 3.5" screen
int CPL = 40; //number of characters each line that a 2.8" screen can contain 

int curRow = 0; 
int offset = 0; 


bool newLineFlag = false;
char Pgbuf[448];
char Msgbuf[50];
char Title[50];
char P[13];
int statsMode = 0;
bool chkStatsMode = true;
bool SwMode = false;
bool BugMode = false;//true;//
bool Bug2 = false;//false;//
bool NrmMode = true;
bool CalcAvgdeadSpace = false;
bool CalcDitFlg = false;
bool LtrBrkFlg = true;
bool NuWrd = true;
bool NuWrdflg = false;
int unsigned ModeCnt = 0; // used to cycle thru the 3 decode modes (norm, bug, bug2)
int unsigned ModeCntRef = 0;
//char TxtMsg1[] = {"1 2 3 4 5 6 7 8 9 10 11\n"};
//char TxtMsg2[] = {"This is my 3rd Message\n"};
int msgcntr = 0;
int badCodeCnt = 0;
int dahcnt =0;
char newLine = '\n';

//volatile bool state = LOW;
volatile bool valid = LOW;
volatile bool mark = LOW;
bool dataRdy = LOW;

volatile long period = 0;
volatile unsigned long start = 0;
volatile unsigned long thisWordBrk = 0;
//volatile long Oldstart=0;//JMH 20190620
volatile unsigned long noSigStrt;
volatile unsigned long WrdStrt;
volatile unsigned long lastWrdVal;
//volatile long OldnoSigStrt;//JMH 20190620
//bool ignoreKyUp = false;//JMH 20190620
//bool ignoreKyDwn = false;//JMH 20190620
//volatile unsigned long deadSpace =0;
volatile int TimeDat[16];
int Bitpos = 0;
volatile int TimeDatBuf[24];
int TDBptr = 0;
volatile unsigned int CodeValBuf[7];
volatile unsigned int DeCodeVal;
volatile unsigned int OldDeCodeVal;
long avgDit = 80; //average 'Dit' duration
unsigned long avgDah = 240; //average 'Dah' duration
//float avgDit = 80; //average 'Dit' duration
//float avgDah = 240; //average 'Dah' duration
float lastDit = avgDit;

int badKeyEvnt = 0;
volatile unsigned long avgDeadSpace = avgDit;
volatile int space = 0;
unsigned long lastDah = avgDah;
volatile unsigned long letterBrk = 0; //letterBrk =avgDit;
unsigned long AvgLtrBrk = 0;
volatile unsigned long ltrBrk = 0;
int shwltrBrk = 0;
volatile unsigned long wordBrk = avgDah ;
volatile unsigned long wordStrt;
volatile unsigned long deadTime;
unsigned long deadSpace = 0;
volatile unsigned long MaxDeadTime;
volatile bool wordBrkFlg = false;
int charCnt = 0;
float curRatio = 3;
int displayW = 320;
//int displayH = 320;
int fontH = 16;
int fontW = 12;
int cursorY = 0;
int cursorX = 0;
int wpm = 0;
int lastWPM = 0;
int state = 0;
//Old Code table; Superceded by two new code tables (plus their companion index tables)
//char morseTbl[] = {
//  '~', '\0',
//  'E', 'T',
//  'I', 'A', 'N', 'M',
//  'S', 'U', 'R', 'W', 'D', 'K', 'G', 'O',
//  'H', 'V', 'F', '_', 'L', '_', 'P', 'J', 'B', 'X', 'C', 'Y', 'Z', 'Q', '_', '_',
//  '5', '4', '_', '3', '_', '_', '_', '2',
//  '_', '_', '_', '_', '_', '_', '_', '1', '6', '-', '/', '_', '_', '_', '_', '_', '7',
//  '_', '_', '_', '8', '_' , '9' , '0'
//};
//Single character decode values/table(s)
// The following two sets of tables are paired.
// The 1st table contains the 'decode' value , & the 2nd table contains the letter /character at the same index row
// so the process is use the first table to find a row match for the decode value, then use the row number 
// in the 2nd table to translate it to the actual character(s) needed to be displayed.   
#define ARSIZE 43
static unsigned int CodeVal1[ARSIZE]={
  5,
  24,
  26,
  12,
  2,
  18,
  14,
  16,
  4,
  23,
  13,
  20,
  7,
  6,
  15,
  22,
  29,
  10,
  8,
  3,
  9,
  17,
  11,
  25,
  27,
  28,
  63,
  47,
  39,
  35,
  33,
  32,
  48,
  56,
  60,
  62,
  49,
  50,
  76,
  255,
  115,
  94,
  85
};

char DicTbl1[ARSIZE][2]=
{
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "-",
    "/",
    "?",
    " ",
    ",",
    "'",
    "."
};
//Multi character decode values/table(s)
#define ARSIZE2 105
static unsigned int CodeVal2[ARSIZE2]={
  19,
  31,
  34,
  38,
  40,
  42,
  44,
  45,
  52,
  54,
  64,
  69,
  70,
  72,
  74,
  78,
  80,
  81,
  82,
  84,
  86,
  89,
  91,
  92,
  96,
  105,
  110,
  113,
  114,
  116,
  118,
  120,
  121,
  122,
  123,
  125,
  126,
  127,
  145,
  146,
  148,
  150,
  162,
  176,
  178,
  192,
  209,
  211,
  212,
  216,
  232,
  234,
  242,
  243,
  244,
  246,
  248,
  283,
  296,
  324,
  328,
  360,
  364,
  416,
  442,
  468,
  482,
  492,
  494,
  500,
  510,
  596,
  708,
  716,
  832,
  842,
  862,
  899,
  922,
  968,
  974,
  1348,
  1480,
  1940,
  1942,
  14752,
  156,
  124,
  493,
  240,
  61,
  88,
  90,
  46,
  6580,
  251,
  59,
  6134,
  502,
  970,
  1785,
  234,
  138,
  790,
  218
};

char DicTbl2[ARSIZE2][5]={
  "UT",
  "TO",
  "VE",
  "UN",
  "<AS>",
  "<AR>",
  "AD",
  "WA",
  "ND",
  "<KN>",
  "HE",
  "<SK>",
  "SG",
  "US",
  "UR",
  "UG",
  "RS",
  "AV",
  "AF",
  "AL",
  "AP",
  "PA",
  "AY",
  "AMI",
  "THE",
  "CA",
  "TAG",
  "GU",
  "GR",
  "MAI",
  "MP",
  "OS",
  "OU",
  "OR",
  "MY",
  "OK",
  "OME",
  "TOO",
  "FU",
  "UF",
  "UL",
  "UP",
  "AVE",
  "WH",
  "PR",
  "THE",
  "CU",
  "CW",
  "AL",
  "YS",
  "QS",  
  "QR",
  "OF",
  "OUT",
  "OL",
  "OP",
  "OB",
  "VY",
  "FB",
  "LL",
  "RRI",
  "WAS",
  "AYI",
  "CH",
  "NOR",
  "MAL",
  "OVE",
  "OAD",
  "OWN",
  "OKI",
  "OON",
  "UAL",
  "WIL",
  "W?",
  "CHE",
  "CAR",
  "CON",
  "<73>",
  "MUC",
  "OUS",
  "OUG",
  "ALL",
  "JUS",
  "OUL",
  "OUP",
  "MUCH",
  "UZ",
  "OD",
  "OAK",
  "OH",
  "OA",
  "AB",
  "AC",
  "WN",
  "XYL",
  "OY",
  "TY",
  "JOYE",
  "OYE",
  "OUR",
  "YOU",
  "YR",
  "VR",
  "NING",
  "YR"
};


//void Timer_ISR(void);
// End of CW Decoder Global Variables

////////////////////////////////////////////////////////////////////

// Install the interrupt routine.
void KeyEvntSR() {
  ChkDeadSpace();
  SetLtrBrk();
  chkChrCmplt();
  if (digitalRead(interruptPin) == LOW) { //key-down
    start = millis();
    //start = micros();
    //unsigned long deadSpace = start-noSigStrt;
    deadSpace = start - noSigStrt;
    letterBrk = 0;
    ltrCmplt = -1800; // used in plot mode, to show where/when letter breaks are detected 
    CalcAvgdeadSpace = true;
    
    if (wordBrkFlg) {
      wordBrkFlg = false;
      thisWordBrk = start - wordStrt;
      //Serial.println(thisWordBrk);
      if (thisWordBrk < 11 * avgDeadSpace) {
        wordBrk = (5 * wordBrk + thisWordBrk) / 6;
        MaxDeadTime = 0;
        charCnt = 0;
        //Serial.println(wordBrk);
      }
    } else if (charCnt > 12) {
      if (MaxDeadTime < wordBrk) {
        wordBrk = MaxDeadTime;
        //Serial.println(wordBrk);
      }
      MaxDeadTime = 0;
      charCnt = 0;
    }
    noSigStrt =  millis();//jmh20190717added to prevent an absurd value
    //noSigStrt = micros();
    if (DeCodeVal == 0) {//we're starting a new symbol set
      DeCodeVal = 1;
      valid = LOW;
    }
    //Test = false;//false;//true;
    return;// end of "keydown" processing
  }
  else { // "Key Up" evaluations;
    if (DeCodeVal != 0) {//Normally true; We're working with a valid symbol set. Reset timers
      noSigStrt =  millis();
      //noSigStrt =  micros();
      period = noSigStrt - start;
      WrdStrt = noSigStrt;
      TimeDat[Bitpos] = period;
      Bitpos += 1;
      if (Bitpos > 15) {
        Bitpos = 15; // watch out: this character has too many symbols for a valid CW character
        letterBrk = noSigStrt; //force a letter brk (to clear this garbage
        return;
      }
     
      start = 0;
    } //End if(DeCodeVal!= 0)

  }// end of key interrupt processing;
  // Now, if we are here; the interrupt was a "Key-Up" event. Now its time to decide whether the "Key-Down" period represents a "dit", a "dah", or just garbage.
  // 1st check. & throw out key-up events that have durations that represent speeds of less than 5WPM.
  if (period > 720) { //Reset, and wait for the next key closure
    //period = 0;
    noSigStrt =  millis();//jmh20190717added to prevent an absurd value
    DeCodeVal = 0;
    return; // overly long key closure; Foregt what you got; Go back, & start looking for a new set of events
  }
  LtrBrkFlg = true; //Arm (enable) SetLtrBrk() routine
  //test to determine that this is a significant signal (key closure duration) event, & warrants evaluation as a dit or dah
  if ((float)period < 0.3 * (float)avgDit) { // if "true", key down event seems to related to noise
    // if here, this was an atypical event, & may indicate a need to recalculate the average period.
    
    if (period > 0) { //test for when the wpm speed as been slow & now needs to speed by a significant amount
      ++badKeyEvnt;
      if (badKeyEvnt >= 20) {
        period = CalcAvgPrd(period);
        //period = 0;
        badKeyEvnt = 0;
        noSigStrt =  millis();//jmh20190717added to prevent an absurd value
        letterBrk = 0;
      }
    }
    //    if(Test) Serial.println(DeCodeVal); //we didn't experience a full keyclosure event on this pass through the loop [this is normal]
    return;
  }

//  if ((float)period > 0.3 * (float)avgDit) { // if "true", we do have a usable event
// if here, its a usable event; Now, decide if its a "Dit" or "Dah"
    badKeyEvnt = 15;//badKeyEvnt = 20;
    DeCodeVal = DeCodeVal << 1; //shift the current decode value left one place to make room for the next bit.
    //if (((period >= 1.8 * avgDit)|| (period >= 0.8 * avgDah))||(DeCodeVal ==2 & period >= 1.4 * avgDit)) { // it smells like a "Dah".
    bool NrmlDah = false;
    if ((period >= 1.8 * avgDit)|| (period >= 0.8 * avgDah)) NrmlDah = true; 
    bool NrmlDit = false;
    //if ((period < 1.3 * avgDit)|| (period < 0.4 * avgDah)) NrmlDit = true;
    //if (((period >= 1.8 * avgDit)|| (period >= 0.8 * avgDah))||(DeCodeVal ==2 & period >= 1.5 * lastDit)) { // it smells like a "Dah".
    if ((NrmlDah)||((DeCodeVal ==2) & (period > 1.4 * avgDit)&Bug2)) { // it smells like a "Dah".  
      //JMH added 20020206
      DeCodeVal = DeCodeVal + 1; // it appears to be a "dah' so set the least significant bit to "one"
      if (NrmlDah) lastDah = period;
      //lastDah = period;
      CalcAvgDah(lastDah);
      dahcnt +=1;
      if(dahcnt>10){
        dahcnt = 3;
        avgDit = int(1.5*float(avgDit));
      }
    }
    else { // if(period >= 0.5*avgDit) //This is a typical 'Dit'
      lastDit = period;
      dahcnt = 0;
      if (DeCodeVal != 2 ) { // don't recalculate speed based on a single dit (it could have been noise)or a single dah ||(curRatio > 5.0 & period> avgDit )
        CalcDitFlg = true;
      }
    }
    period = CalcAvgPrd(period);
    //period = 0;
    return; // ok, we are done with evaluating this usable key event
}// End CW Decoder Interrupt routine
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/* Call this routine before every "block" (size=N) of samples. */
void ResetGoertzel(void)
{
  Q2 = 0;
  Q1 = 0;
  Q2C = 0;
  Q1C = 0;
  Q2H = 0;
  Q1H = 0;
}

/* Call this once, to precompute the constants. */
void InitGoertzel(void)
{
  int  k;
  FLOATING  floatN;
  FLOATING  omega;
  //  N = (int)((SAMPLING_RATE/TARGET_FREQUENCY)*(float)(CYCLE_CNT));
  //  floatN = (FLOATING) N;
  //
  //  k = (int) (0.5 + ((floatN * TARGET_FREQUENCY) / SAMPLING_RATE));
  //  omega = (2.0 * PI * k) / floatN;
  //  sine = sin(omega);
  //  cosine = cos(omega);
  //  coeff = 2.0 * cosine;

  NC = (int)((SAMPLING_RATE / TARGET_FREQUENCYC) * (float)(CYCLE_CNT));
  floatN = (FLOATING) NC;

  k = (int) (0.5 + ((floatN * TARGET_FREQUENCYC) / SAMPLING_RATE));
  omega = (2.0 * PI * k) / floatN;
  //  sine = sin(omega);
  cosine = cos(omega);
  coeffC = 2.0 * cosine;

  NL = (int)((SAMPLING_RATE / TARGET_FREQUENCYL) * (float)(CYCLE_CNT));
  floatN = (FLOATING) NL;

  k = (int) (0.5 + ((floatN * TARGET_FREQUENCYL) / SAMPLING_RATE));
  omega = (2.0 * PI * k) / floatN;
  //  sine = sin(omega);
  cosine = cos(omega);
  coeffL = 2.0 * cosine;

  NH = (int)((SAMPLING_RATE / TARGET_FREQUENCYH) * (float)(CYCLE_CNT));
  floatN = (FLOATING) NH;

  k = (int) (0.5 + ((floatN * TARGET_FREQUENCYH) / SAMPLING_RATE));
  omega = (2.0 * PI * k) / floatN;
  //  sine = sin(omega);
  cosine = cos(omega);
  coeffH = 2.0 * cosine;
  ResetGoertzel();
}

/* Call this routine for every sample. */
void ProcessSample(int sample, int Scnt)
{
  FLOATING Q0;
  FLOATING FltSampl = (FLOATING)sample;
  Q0 = (coeffC * Q1C) - Q2C + FltSampl; //samplerate = 21630;  max centerfreq out single freq
  if (Scnt < NC) {
    Q2C = Q1C;
    Q1C = Q0;
  }
  //  if (Scnt == NH-1){ //The Highest Freq has the fewest number of samples
  //    Q2H = Q2C;
  //    Q1H = Q1C;
  //  }
  Q0 = (coeffH * Q1H) - Q2H + FltSampl; //samplerate = 14205;   max centerfreq out two freq
  if (Scnt < NH) { //The Highest Freq has the fewest number of samples
    Q2H = Q1H;
    Q1H = Q0;
  }
  Q0 = (coeffL * Q1) - Q2 + FltSampl;; //samplerate = 10520;   max centerfreq out three freq
  Q2 = Q1;
  Q1 = Q0;
}


/* Optimized Goertzel */
/* Call this after every block to get the RELATIVE magnitude squared. */
FLOATING GetMagnitudeSquared(float q1, float q2, float Coeff)
{
  FLOATING result;

  result = (q1 * q1) + (q2 * q2) - (q1 * q2 * Coeff);
  return result;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
void readResistiveTouch(void)
{
  
  if (CurCnt > NL){
    tchcnt = 100;
    BlkIntrpt = true;  
    tp = ts.getPoint();
    if (tp.z < 200){ // user is NOT pressing on the Display
      btnPrsdCnt = 0;
      buttonEnabled = true;
      SwMode = false;
      //ensure that the touch point no longer represents a valid point on the display
      px = 1000; 
      py = 1000;
    } 
    //Just did a touch screen check, so restore shared LCD pins
    pinMode(YP, OUTPUT);
    pinMode(XM, OUTPUT);
    digitalWrite(YP, HIGH);
    digitalWrite(XM, HIGH);
    BlkIntrpt = false;
  }else{
    tchcnt = 1;
  }
   // Serial.println("tp.x=" + String(tp.x) + ", tp.y=" + String(tp.y) + ", tp.z =" + String(tp.z));
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
// Define Timer2 ISR
void Timer_ISR(void) {
  if(BlkIntrpt)return;
  if (CurCnt <= NL) {//if true; collect a sound data point
    //digitalWrite(DataOutPin, HIGH); //for timing/tuning tests only
    int k = analogRead(PB1);    // read the ADC value from pin PB1; (k = 0 to 4096)
    k -= 1987; // form into a signed int
//    if (k > 2000 | k < -2000) {
//      k = kOld; //bad data point zero it out
//      OvrLd = true;
//    }
//    else kOld = k;
    ProcessSample(k, CurCnt);
    //Serial.println(k);// enable to graph raw DAC values
    k = abs(k);
    AvgVal += (float)k;
    CurCnt += 1;
    return;
  }
  //Enough data points taken, now process Sound data for a valid tone
  //digitalWrite(DataOutPin, LOW); //for timing/tuning tests only
  mag = sqrt(GetMagnitudeSquared(Q1C, Q2C, coeffC));
  bool GudTone = true;
  if (mag < 2000) GudTone = false; //JMH20200128
  //float CurNoise = ((CurNoise) + ((0.8 * AvgVal) - mag)) / 2;
  float CurNoise = ((CurNoise) + ((1.6*AvgVal) - mag)) / 2;
  ToneLvl = mag;
  //noise = ((6 * noise) + (CurNoise)) / 7; // calculate the running average of the unfiltered digitized Sound
  noise = ((4 * noise) + (CurNoise)) / 5; // calculate the running average of the unfiltered digitized Sound
  if (ToneLvl > noise & ToneLvl >  CurNoise) {
    ++ToneOnCnt;
    if (ToneOnCnt >= 4) {
      ToneOnCnt = 3;
      //MidPt =  (MidPt+(((ToneLvl -noise)/2)+ noise))/2;//NoiseAvgLvl
      MidPt =  ((2 * MidPt) + (((ToneLvl - NoiseAvgLvl) / 4) + NoiseAvgLvl)) / 3;
      if (MidPt >  AvgToneSqlch) AvgToneSqlch = MidPt;
    }
  }
  SqlchVal = noise;
  if ((ToneLvl < CurNoise) & (CurNoise > SqlchVal))  SqlchVal = CurNoise; //could be a static burst
  if (AvgToneSqlch > SqlchVal) SqlchVal = AvgToneSqlch;
  if ( ToneLvl > SqlchVal) {
    magC = (14 * magC + mag) / 15;
    magL = (14 * magL + (sqrt(GetMagnitudeSquared(Q1, Q2, coeffL)))) / 15;
    magH = (14 * magH + (sqrt(GetMagnitudeSquared(Q1H, Q2H, coeffH)))) / 15;
  }

  ////////////////////////////////////////////////////////////
  //TonPltFlg = true;// comment out this line when you don't want graph/plot tone data 
  if (TonPltFlg) {
    // Graph Geortzel magnitudes
    Serial.print(magH);//Blue
    Serial.print("\t");
    Serial.print(magL);//Red
    Serial.print("\t");
    Serial.print(magC);//Green
    Serial.print("\t");

    // Control Sigs
    Serial.print(ToneLvl);//Serial.print(mag);//Serial.print(ToneLvl);//Orange
    Serial.print("\t");
    //    Serial.print(NoiseAvgLvl);//Purple
    //    Serial.print("\t");
    Serial.print(noise);//Purple
    Serial.print("\t");
    Serial.print(AvgToneSqlch);//Gray
    Serial.print("\t");
  }
  if (( ToneLvl > SqlchVal)) {
    toneDetect = true; 
  } else toneDetect = false;

//  if (armHi && ( ToneLvl > SqlchVal)) {
//    toneDetect = true; //Had the same Tonestate 2X in a row. So go with it
//  }
//  if (armLo && ( ToneLvl < SqlchVal)) {
//    toneDetect = false; //Had the same Tonestate 2X in a row. So go with it
//    //TnLpCnt = 0;
//  }
//  if (!armHi && ( ToneLvl > SqlchVal)) { //+50
//    armHi = true;
//    armLo = false;
//    //toneDetect = true;
//  }
//  if (!armLo && ( ToneLvl < SqlchVal)) { //+50
//    armHi = false;
//    armLo = true;
//    //toneDetect = false;
//    ToneOnCnt = 0;
//    //TnLpCnt = 0;
//  }
  loopcnt -= 1;
  if (loopcnt < 0) {
    NoiseAvgLvl = (30 * NoiseAvgLvl + (noise)) / 31; //every 3rd pass thru, recalculate running average of the composite noise signal
    //AvgToneSqlch= (30*AvgToneSqlch +noise)/31;
    //AvgToneSqlch= (17*AvgToneSqlch +noise)/18;//trying a little faster decay rate, to better track QSB
    AvgToneSqlch = (17 * AvgToneSqlch + NoiseAvgLvl) / 18;
    loopcnt = 2;
  }
  delayLine = delayLine << 1;
  delayLine &= B00001110;
  delayLine2 = delayLine2 << 1; //2nd delay line used pass led info to RGB LED
  delayLine2 &= B00001110;
  if (GudTone) delayLine2 |= B00000001;
  if (toneDetect) delayLine |= B00000001;
  //Use For Slow code [<27WPM] fill in the glitches  pin is left open
  if (1) { //if(digitalRead(SlowData)){
    if (((delayLine ^ B00001110) == 4 ) || ((delayLine ^ B00001111) == 4)) delayLine |= B00000100;
    if (((delayLine ^ B00000001) == B00000100) || ((delayLine ^ B00000000) == B00000100)) delayLine &= B11111011;
  }
  KeyState = -2000;
  
  if (delayLine & B00001000) { // key Closed
    digitalWrite(DataOutPin, LOW);
    KeyState = -250;
  }
  else { //No Tone Detected
    digitalWrite(DataOutPin, HIGH);
  }
  
  if (TonPltFlg) {
    // Add the final entry to the plot data
    Serial.print(KeyState);//Light Blue
    Serial.print("\t");
    Serial.println(ltrCmplt);//Light ??
    //TonPltFlg = false;
  }

  //The following code is just to support the RGB LED.
  if (delayLine & B00001000) { //key Closed
    LEDGREEN = 0;
    LEDRED = 0;
    LEDBLUE = 0;
    if (magC > 100000) { //Excessive Tone Amplitude
      LEDGREEN = 120;
      LEDRED = 120;
      LEDBLUE = 120;
    }//End Excessive Tone Amplitude
    else { // modulated light tests
      LightLvl = (byte)(256 * (magC / 100000));
      //Serial.println(LightLvl);
      if (magC < 1500) { // Just detectable Tone
        LEDGREEN = LightLvl;
        LEDRED = LightLvl;
        LEDBLUE = LightLvl;
      }//End Just detectable Tone
      else { //Freq Dependant Lighting
        magC2 = magC;//1.02*magC;
        if (magC2 >= magH & magC2 >= magL) { //Freq Just Right
          //BLUE = 0;
          LEDGREEN = LightLvl;
          //RED = 0;
        }//End Freq Just Right
        else if ((magH > magC) & (magH >= magL)) { //Freq High
          LEDBLUE = LightLvl;
          //GREEN= 0;
          //RED = 0;
        }//End Freq High
        else if ((magL > magC) & (magL >= magH)) { //Freq Low
          LEDRED = LightLvl;
          //GREEN= 0;
          //BLUE = 0;
        }//End Freq Low
        if (LEDGREEN == 0 & LEDRED == 0 & LEDBLUE == 0) {
          LEDGREEN = 10;
          LEDRED = 10;
          LEDBLUE = 10;
        }
      }//End Freq Dependant Lighting

    }//End modulated light tests
//    strip.setPixelColor(0, strip.Color(LEDGREEN, LEDRED, LEDBLUE));
  }//end Key Closed Tests
  else { //key open
    LEDGREEN = 0;
    LEDRED = 0;
    LEDBLUE = 0;
//    if (OvrLd) {
//      LEDGREEN = 10;
//      LEDRED = 5;
//      LEDBLUE = 10;
//    }
//    else {
//      LEDGREEN = 0;
//      LEDRED = 0;
//      LEDBLUE = 0;
//    }

//    strip.setPixelColor(0, strip.Color(LEDGREEN, LEDRED, LEDBLUE)); // set color of ASD 1293 LED to Green
  }
  strip.setPixelColor(0, strip.Color(LEDGREEN, LEDRED, LEDBLUE));
  strip.show(); //activate the RGB LED
  // this round of sound processing is complete, so setup to start to collect the next set of samples
  ResetGoertzel();
  AvgVal = 0.0;
  OvrLd = false;
  CurCnt = 0;
} //End Timer2 ISR
///////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  //setup Digital signal output pin
  pinMode(DataOutPin, OUTPUT);
  digitalWrite(DataOutPin, HIGH);
  pinMode(PB1, INPUT);
  Serial.begin(115200); // use the serial port
  strip.begin();

  InitGoertzel();
  coeff = coeffC;
  N = NC;

  //Setup & Enable Timer ISR (in this case Timer2)
  Timer2.setMode(TIMER_CH1, TIMER_OUTPUTCOMPARE);
  Timer2.setPeriod(SAMPL_RATE); // in microseconds
  Timer2.setCompare(TIMER_CH1, 1); // overflow might be small
  Timer2.attachInterrupt(TIMER_CH1, Timer_ISR);
  // End of Goertzel Tone Processing setup
  //Begin CW Decoder setup
  tft.reset();
  ID = 0x9486;// MCUfriends.com 2.8" Display id;
  //  ID = 0x9341;// MCUfriends.com 2.8" Display id; (White Letters on Black Screen)
  //ID = 0x6814;// MCUfriends.com 3.5" Display id
  //  ID = 0x9090;//MCUfriends.com 3.5" Display id (Black Letters on White Screen)
  //  ID = 0x4747;//MCUfriends.com 2.8" Display id
  tft.begin(ID);//  The value here is screen specific & depends on the chipset used to drive the screen,
  tft.setRotation(1);
  tft.fillScreen(BLACK);
  scrnHeight = tft.height();
  scrnWidth = tft.width();
  displayW = scrnWidth;
//  if (scrnHeight == 320) { //we are using a 3.5 inch screen
//    row = 10; //max number of decoded text rows that can be displayed
//    ylo = 0; //button touch reading 3.5" screen
//    yhi = 40; //button touch reading 3.5" screen
//    Stxl = 0; //Status touch reading 3.5" screen
//    Stxh = 150; //Status touch reading 3.5" screen
//    bxlo = 155; //button touch reading 3.5" screen
//    bxhi = 236; //button touch reading 3.5" screen
//    gxlo  = 238; //button touch reading 3.5" screen
//    gxhi  = 325; //button touch reading 3.5" screen
//   }

  DrawButton();
  Button2();
  WPMdefault();
  tft.setCursor(textSrtX, textSrtY);
  tft.setTextColor(WHITE);//tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(2);
  tft.setTextWrap(false);

  enableINT(); //This is a local function, and is defined below

  start = 0;
  WrdStrt = millis();
//  Serial.print("Starting...");
  sprintf( Title, "             KW4KD (%s)           ", RevDate );
//  if (scrnHeight == 320) { //we are using a 3.5 inch screen
//    sprintf( Title, "             KW4KD (%s)           ", RevDate );
//    
//  }
//  else {
//    sprintf( Title, "      KW4KD (%s)     ", RevDate );
//    
//  }
  dispMsg(Title);
  wordBrk = ((5 * wordBrk) + 4 * avgDeadSpace) / 6;
  //End CW Decoder Setup
}


/*
   MAIN LOOP &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
*/
void loop()
{
  ChkDeadSpace();
  SetLtrBrk();
  if (CalcDitFlg) { // moved this from interupt routine to reduce time needed to process interupts
    CalcDitFlg = false;
    avgDit = (5 * avgDit + lastDit) / 6;
    curRatio = (float)avgDah / (float)avgDit;
  }
  tchcnt -=1;
  if(tchcnt==0){
    readResistiveTouch();
  } else tp = {0, 0, 0};
  //if(!ScrnPrssd & btnPrsdCnt > 0) btnPrsdCnt = 0;  
  if (tp.z > 200) { // if (tp.z > MINPRESSURE && tp.z < MAXPRESSURE) { //
    py = map(tp.y, TS_TOP, TS_BOT, 0, scrnHeight);
    px = map(tp.x, TS_LEFT, TS_RT, 0, scrnWidth);
//    Serial.print("tp.z = "); Serial.print(tp.z);
//    Serial.print("\t; px: "); Serial.print(px);
//    Serial.print("\t; py: "); Serial.println(py);
//    buttonEnabled = false;
//    If (NoTchCnt > 0){
//      buttonEnabled = true
//      NoTchCnt = 0;
//    }
    
    if (px > Stxl && px < Stxh && py > ylo && py < yhi && !SwMode) { //User has touched a point inside the status area, & wants to change WPM/avgtiming
      //Note: SwMode gets reset to "false" in 'ShowSpeed' routine, after user quits pressing the status area of the screen.
      //Serial.println("Status Area");
      if (btnPrsdCnt < 10) { // press screen 'debounce' interval
        btnPrsdCnt += 1;
        //Serial.println(btnPrsdCnt);
      }
      else { // user pressed screen long enough to indicate real desire to change/flip the status info
        if(btnPrsdCnt == 10){
          SwMode = true;
          //Serial.println("Status Area");
          if (statsMode == 0) {
            statsMode = 1;
            //Serial.println("SET statsMode =1");
          } else {
            statsMode = 0;
            //Serial.println("SET statsMode =0");
          }
          showSpeed();  
        }
        else btnPrsdCnt = 10;
      }

    }
  }//end tft display "touch" check

    if (px > bxlo && px < bxhi && py > ylo && py < yhi && buttonEnabled) // The user has touched a point inside the Blue rectangle, & wants to clear the Display
    { //reset (clear) the Screen
      buttonEnabled = false; //Disable button
      //   Serial.print("Clear Screen");
  //    if ( interruptPin == 2) KillINT();
  //    else enableDisplay();
      enableDisplay();
      tft.fillScreen(BLACK);
      DrawButton();
      Button2();
      WPMdefault();
//      avgDit = 80; //average 'Dit' duration
//      avgDeadSpace = avgDit;
//      AvgLtrBrk = 3*avgDit;
//      avgDah = 240;
//      wpm = CalcWPM(avgDit);
      px = 0;
      py = 0;
      showSpeed();
      tft.setCursor(textSrtX, textSrtY);
  //    if ( interruptPin == 2) enableINT();
      for ( int i = 0; i <  sizeof(Pgbuf);  ++i )
        Pgbuf[i] = 0;
      cnt = 0;
      curRow = 0;
      offset = 0;
      cursorX = 0;
      cursorY = 0;
    } else if (px > gxlo && px < gxhi && py > ylo && py < yhi && buttonEnabled) { //User has touched a point inside the Green Mode rectangle, & wants to change Mode (normal/bug1/bug2)
      px = 0; //kill button press
      py = 0; //kill button press
      buttonEnabled = false;
      ModeCnt += 1;
      if (ModeCnt >= 3) ModeCnt = 0;
      ModeCntRef = ModeCnt;
      SetModFlgs(ModeCnt);
      enableDisplay();
      Button2();
    }

  chkChrCmplt();
  while (CodeValBuf[0] > 0) {
    DisplayChar(CodeValBuf[0]);
  }
  SetModFlgs(ModeCnt);//if(ModeCntRef != ModeCnt) ModeCnt= ModeCntRef; //added this in an attempt to fix mysterious loss of value in ModeCnt

}//end of Main Loop

//////////////////////////////////////////////////////////////////////////
void WPMdefault()
{
    avgDit = 80.0; //average 'Dit' duration
    avgDeadSpace = avgDit;
    avgDah = 240.0;
    AvgLtrBrk = avgDah;
    wpm = CalcWPM(avgDit, avgDah, avgDeadSpace);
}


void ChkDeadSpace(void)
{
  if (CalcAvgdeadSpace) { // Just detected the start of a new keydown event
    CalcAvgdeadSpace = false;
    if (NuWrd) lastWrdVal = millis() - WrdStrt;
    //if (NuWrd) lastWrdVal = micros() - WrdStrt;
    if ((deadSpace > 15) && (deadSpace < 240) && (!NuWrd)) { // looks like we have a legit dead space interval(its between 5 & 50 WPM)
      if (Bug2) {
        if (deadSpace < avgDit && deadSpace > avgDit / 4) {
          avgDeadSpace = (15 * avgDeadSpace + deadSpace) / 16;
        }
      } else {
        //if ((deadSpace < wordBrk)) {
        if ((deadSpace < lastDah)) {
          //if (DeCodeVal != 1) { //ignore letterbrk dead space intervals
          if(ltrCmplt < -350){  //ignore letterbrk dead space intervals
            //avgDeadSpace = (3 * avgDeadSpace + deadSpace) / 4;
            avgDeadSpace = (7 * avgDeadSpace + deadSpace) / 8;
          } else AvgLtrBrk = ((9 * AvgLtrBrk) + deadSpace) / 10;
        }
        if (NrmMode && (avgDeadSpace < avgDit)) { // running Normall mode; use Dit timing to establish minmum "space" interval
          if(ltrCmplt < -350) avgDeadSpace = avgDit; //ignore letterbrk dead space intervals
          //Serial.println("Path 3");
        }
      }
    }
    //    Serial.print("; ");
    //    Serial.print(avgDeadSpace);
    //    Serial.print("; ");
    //    Serial.print(avgDit);
    //    Serial.print("; ");
    //    Serial.print(avgDah);
    //    Serial.print("; ");
    //    Serial.print(ltrBrk);
    //    Serial.print("; ");
    //    Serial.print(AvgLtrBrk);
    //    Serial.print("; ");
    //    Serial.print(wordBrk);
    //    Serial.print("; ");
    //    Serial.print(lastWrdVal);
    //    Serial.print("; ");
    //    Serial.print(NuWrd);
    //    Serial.print("; ");
    //    Serial.println(DeCodeVal);
    if (NuWrd) NuWrdflg = true;;
  }
}

///////////////////////////////////////////////////////////////////////
void SetLtrBrk(void)
{
  unsigned long ltrBrka;
  unsigned long ltrBrkb;
  if (!LtrBrkFlg) return;
// Just detected the start of a new keyUp event
  LtrBrkFlg = false;
  //Figure out how much time to allow for a letter break
  if (Bug2) {
    space = ((3 * space) + avgDeadSpace) / 4;
  }
  else {
    if (avgDeadSpace > avgDit) space = avgDeadSpace; //((3*space)+avgDeadSpace)/4; //20190717 jmh - Changed to averaging space value to reduce chance of glitches causing mid character letter breaks
    else space =  ((3 * space) + avgDit) / 4; //20190717 jmh - Changed to averaging space value to reduce chance of glitches causing mid character letter breaks
  }
  
  if(wpm<35){
    ltrBrk =  int(1.6*(float(space))); //int(1.7*(float(space))); //int(1.5*(float(space))); // Basic letter break interval //20200220 changed to 1.6 becuase was getting "L" when it should hve read "ED"
    //ltrBrk = avgDeadSpace + ((AvgLtrBrk - avgDeadSpace) / 2.0);
    if (BugMode) { //use special case spacing
      if (((DeCodeVal & 1) == 1) && (DeCodeVal > 3)) { //this dead space interval appears to be an mid-character event AND the last symbol detected was a "DAH".
        ltrBrk = int(2.5*(float(space)));//int(1.8*(float(space)));
        //ltrBrk = avgDeadSpace + ((AvgLtrBrk - avgDeadSpace) / 2.0);
      } else if ((deadSpace < wordBrk) && (DeCodeVal == 3)) { // the first symbol sent is a dash
          // At this point(time) the letter looks like a "T", but it could be the beginning of something else;i.e., "N"
          ltrBrka = long(float(avgDah) * 0.73);
          ltrBrkb = long(float(avgDeadSpace) * 2.1);
        if (ltrBrka >= ltrBrkb) { //hold on, new ltrBrk interval seems short
          ltrBrk = ltrBrka;
        } else {
          ltrBrk = ltrBrkb;
        }
      }
    }
  }
  // if here, setup "letter break" timing for speeds greater than 35 wpm
  else ltrBrk = int(2.1*(float(space)));//int(1.6*(float(space)));//int(1.6*(float(avgDeadSpace)));
  
  if (ltrBrk > wordBrk) {
    wordBrk = int(1.1 * float(ltrBrk));
  }

  letterBrk = ltrBrk + noSigStrt; //set the next letter break "time stamp"
  if (BugMode) letterBrk = letterBrk + 0.8 * avgDit ;
  if (NuWrd && NuWrdflg) {
    NuWrd = false;
    NuWrdflg = false;
  }
}
////////////////////////////////////////////////////////////////////////
void chkChrCmplt() {
  state = 0;
  unsigned long now = millis();
  //unsigned long now = micros();
  //check to see if enough time has passed since the last key closure to signify that the character is complete
  if ((now >= letterBrk) && letterBrk != 0 && DeCodeVal > 1) {
    state = 1; //have a complete letter
    ltrCmplt = -350;

    //Serial.println("   ");//testing only; comment out when running normally
    //Serial.println(now-letterBrk);
    //Serial.println(letterBrk);
  }
  float noKeySig = (float)(now - noSigStrt);
  if ((noKeySig >= 0.75 * ((float)wordBrk) ) && noSigStrt != 0 && !wordBrkFlg && (DeCodeVal == 0)) {
    //Serial.print(wordBrk);
    //Serial.print("\t");
    state = 2;//have word

    wordStrt = noSigStrt;
    if ( DeCodeVal == 0) {
      noSigStrt =  millis();//jmh20190717added to prevent an absurd value
      //noSigStrt =  micros();
      MaxDeadTime = 0;
      charCnt = 0;
    }
    wordBrkFlg = true;
    
  }

  // for testing only
  //  if(OldDeCodeVal!=DeCodeVal){
  //    OldDeCodeVal=DeCodeVal;
  //    Serial.print(DeCodeVal);
  //    Serial.print("; ");
  //  }

  if (state == 0) {
    if ((unsigned long)noKeySig > MaxDeadTime && noKeySig < 7 * avgDeadSpace ) MaxDeadTime = (unsigned long)noKeySig;
    return;
  }
  else {

    if (state >= 1) { //if(state==1){

      if (DeCodeVal >= 2) {
        int i = 0;
        while (CodeValBuf[i] > 0) {
          ++i;// move buffer pointer to 1st available empty array position
          if (i == 7) { // whoa, we've gone too far! Stop, & force an end to this madness
            i = 6;
            CodeValBuf[i] = 0;
          }
        }
        CodeValBuf[i] = DeCodeVal;
        /*
        Serial.print(CodeValBuf[i]);// for testing only
        Serial.print(";  i:");
        Serial.println(i);
        */
        for (int p = 0;  p < Bitpos; p++ ) { // map timing info into time buffer (used only for debugging
          TimeDatBuf[p] = TimeDat[p];
          TimeDat[p] = 0; // clear out old time data
        }
      }
      //      Serial.print("DeCodeVal = "); Serial.println(DeCodeVal);
      if (state == 2) {
        int i = 0;
        while (CodeValBuf[i] > 0) ++i; // move buffer pointer to 1st available empty array position
        CodeValBuf[i] = 255; //insert space in text to create a word break
        NuWrd = true;
      }
      TDBptr = Bitpos;
      Bitpos = 0;

      letterBrk = 0;
      ++charCnt;
      DeCodeVal = 0; // make program ready to process next series of key events
      period = 0;//     before attemping to display the current code value
    }
  }
}
//////////////////////////////////////////////////////////////////////


int CalcAvgPrd(int thisdur) {

  if (thisdur > 3.4 * avgDit) thisdur = 3.4 * avgDit; //limit the effect that a single sustained "keydown" event cant have
  if (thisdur < 0.5 *avgDah){ // this appears to be a "dit"
    avgDit = (9 * avgDit + thisdur) / 10;
  }
  curRatio = (float)avgDah / (float)avgDit;
  //set limits on the 'avgDit' values
  if (avgDit > 1000) avgDit = 1000;
  if (avgDit < 15) avgDit = 15;
  if (DeCodeVal == 1) DeCodeVal = 0;
  //      if(Test){
  //        Serial.print(DeCodeVal);
  //        Serial.print(";  ");
  //        Serial.println("Valid");
  //      }
  thisdur = avgDah / curRatio;
  return thisdur;
}

/////////////////////////////////////////////////////////////////////

int CalcWPM(int dotT, int dahT, int spaceT) {
  int avgt = (dotT+ dahT+ spaceT)/5;
  int codeSpeed = 1200 / avgt;
  return codeSpeed;
}

///////////////////////////////////////////////////////////////////////

void CalcAvgDah(int thisPeriod) {
  if (NuWrd) return; //{ //don't calculate; period value is not reliable following word break
  avgDah = int((float(9 * avgDah) + thisPeriod) / 10);
  
}
/////////////////////////////////////////////////////////////////////////
void SetModFlgs(int ModeVal) {
  switch (ModeVal) {
    case 0:
      BugMode = false;
      Bug2 = false;
      NrmMode = true;
      break;
    case 1:
      BugMode = true;
      NrmMode = false;
      break;
    case 2:
      BugMode = false;
      Bug2 = true;
      NrmMode = false;
      break;
  }
}
//////////////////////////////////////////////////////////////////////

void DisplayChar(unsigned int decodeval) {
  char curChr = 0 ;
  int pos1 = -1;
  // slide all values in the CodeValBuf to the left by one position & make sure that the array is terminated with a zero in the last position
  for (int i = 1; i < 7; i++) {
    CodeValBuf[i - 1] = CodeValBuf[i];
  }
  CodeValBuf[6] = 0;

  if (decodeval == 2 || decodeval == 3) ++TEcnt;
  else TEcnt = 0;
  if (Test) Serial.print(decodeval);
  if (Test) Serial.print("\t");
  //clear buffer
  for ( int i = 0; i < sizeof(Msgbuf);  ++i )
    Msgbuf[i] = 0;
  pos1 = linearSearchBreak(decodeval, CodeVal1, ARSIZE);
  if(pos1<0){
    pos1 = linearSearchBreak(decodeval, CodeVal2, ARSIZE2);
    if(pos1<0){
      //sprintf( Msgbuf, "decodeval: %d ", decodeval);//use when debugging Dictionary table(s)
      sprintf( Msgbuf, "*"); //Enable for Normal running 
    }
    else sprintf( Msgbuf, "%s", DicTbl2[pos1] );
  }
  else sprintf( Msgbuf, "%s", DicTbl1[pos1] );
  if (Msgbuf[0] == 'E' || Msgbuf[0] == 'T') ++badCodeCnt;
  else if(decodeval !=255) badCodeCnt = 0;
  if (badCodeCnt > 5 && wpm > 25){ // do an auto reset back to 15wpm
    WPMdefault();
  }
  if (((cnt) - offset)*fontW >= displayW) {
    ;//if ((cnt -(curRow*LineLen) == LineLen)){
    curRow++;
    offset = cnt;
    cursorX = 0;
    cursorY = curRow * (fontH + 10);
    tft.setCursor(cursorX, cursorY);
    if (curRow + 1 > row)scrollpg(); // its time to Scroll the Text up one line
  }
  //sprintf ( Msgbuf, "%s%c", Msgbuf, curChr );
  dispMsg(Msgbuf); // print current character(s) to OLED display
  int avgntrvl = int(float(avgDit + avgDah) / 4);
  //wpm = CalcWPM(avgntrvl);//use all current time intervalls to extract a composite WPM
  wpm = CalcWPM(avgDit, avgDah, avgDeadSpace);
  if (wpm != 1) { //if(wpm != lastWPM & decodeval == 0 & wpm <36){//wpm != lastWPM
    if (curChr != ' ') showSpeed();
    if (TEcnt > 7 &&  curRatio > 4.5) { // if true, we probably not waiting long enough to start the decode process, so reset the dot dash ratio based o current dash times
      avgDit = avgDah / 3;
    }
  }
}
//////////////////////////////////////////////////////////////////////
int linearSearchBreak(long val, unsigned int arr[], int sz)
{
  int pos = -1;
  for (int i = 0; i < sz; i++)
  {
    if (arr[i] == val)
    {
      pos = i;
      break;
    }
  }
  return pos;
}

//////////////////////////////////////////////////////////////////////
void dispMsg(char Msgbuf[50]) {
  if (Test) Serial.println(Msgbuf);
  int msgpntr = 0;
  //  if ( interruptPin == 2) KillINT();
  //  else enableDisplay();
  enableDisplay();
  tft.setCursor(cursorX, cursorY);
  while ( Msgbuf[msgpntr] != 0) {
    char curChar = Msgbuf[msgpntr];
    if (curRow > 0) sprintf ( Pgbuf, "%s%c", Pgbuf, curChar);
    tft.print(curChar);
    msgpntr++;
    cnt++;
    if (((cnt) - offset)*fontW >= displayW) {
      curRow++;
      cursorX = 0;
      cursorY = curRow * (fontH + 10);
      offset = cnt;
      tft.setCursor(cursorX, cursorY);
      if (curRow + 1 > row) {
        scrollpg();
      }
    }
    else cursorX = (cnt - offset) * fontW;
  }
  ChkDeadSpace();
  SetLtrBrk();
  chkChrCmplt();
}
//////////////////////////////////////////////////////////////////////
void scrollpg() {
  //buttonEnabled =false;
  curRow = 0;
  cursorX = 0;
  cursorY = 0;
  cnt = 0;
  offset = 0;
  enableDisplay();
  tft.fillRect(cursorX, cursorY, displayW, row * (fontH + 10), BLACK); //erase current page of text
  tft.setCursor(cursorX, cursorY);
  while (Pgbuf[cnt] != 0 && curRow + 1 < row) { //print current page buffer and move current text up one line
    ChkDeadSpace();
    SetLtrBrk();
    chkChrCmplt();
    tft.print(Pgbuf[cnt]);
//    if (displayW == 480) Pgbuf[cnt] = Pgbuf[cnt + 40]; //shift existing text character forward by one line
//    else Pgbuf[cnt] = Pgbuf[cnt + 27]; //shift existing text character forward by one line
    Pgbuf[cnt] = Pgbuf[cnt + CPL]; //shift existing text character forward by one line
    cnt++;
    //delay(300);
    if (((cnt) - offset)*fontW >= displayW) {
      curRow++;
      offset = cnt;
      cursorX = 0;
      cursorY = curRow * (fontH + 10);
      tft.setCursor(cursorX, cursorY);
    }
    else cursorX = (cnt - offset) * fontW;

  }//end While Loop
  while (Pgbuf[cnt] != 0) { //finish cleaning up last line
    chkChrCmplt();
    //tft.print(Pgbuf[cnt]);
    Pgbuf[cnt] = Pgbuf[cnt + 26];
    cnt++;
  }
}
/////////////////////////////////////////////////////////////////////


void enableINT() {
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), KeyEvntSR, CHANGE);
}

void enableDisplay() {
  //This is important, because the libraries are sharing pins
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
}

void DrawButton() {
  //Create Clear Button
  int Bposx = 130;
  int Bwidth = 80;
  int Bposy = 195;
  int Bheight = 40;
  if (scrnHeight == 320) Bposy = scrnHeight - (Bheight + 5);
  if (scrnWidth == 480)  Bposx = Bposx + 32; //Bposx = Bposx +20;
  tft.fillRect(Bposx, Bposy, Bwidth, Bheight, BLUE);
  tft.drawRect(Bposx, Bposy, Bwidth, Bheight, WHITE);
  tft.setCursor(Bposx + 11, Bposy + 12);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.print("Clear");

}
///////////////////////////////////////////////////////////

void Button2() {
  //Create Norm/Bug Button
  int Bposx = 210;
  int Bwidth = 80;
  int Bposy = 195;
  int Bheight = 40;
  if (scrnHeight == 320) Bposy = scrnHeight - (Bheight + 5);
  if (scrnWidth == 480) Bposx = Bposx + 32; //Bposx = Bposx +20;
  tft.fillRect(Bposx, Bposy, Bwidth, Bheight, GREEN);
  tft.drawRect(Bposx, Bposy, Bwidth, Bheight, WHITE);
  tft.setCursor(Bposx + 11, Bposy + 12);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  switch (ModeCnt) {
    case 0:
      tft.print("Norm");
      break;
    case 1:
      tft.print("Bug1");
      break;
    case 2:
      tft.print("Bug2");
      break;
    default:
      tft.print(ModeCnt);
      break;
  }
  //  if(!BugMode) tft.print("Norm");
  //  else  tft.print("Bug");
}
///////////////////////////////////////////////////////////


void showSpeed(){
  char buf[20];
  int ratioInt = (int)curRatio;
  int ratioDecml = (int)((curRatio - ratioInt) * 10);
  chkStatsMode = true;
  //if (SwMode && buttonEnabled) SwMode = false;
  if (statsMode == 0) {
    //  sprintf ( buf,"%d/%d.%d/%d", wpm, ratioInt, ratioDecml, avgDeadSpace );
    sprintf ( buf, "%d/%d.%d WPM", wpm, ratioInt, ratioDecml);
    //  sprintf ( buf,"%d",wordBrk);
  }
  else {
    sprintf ( buf, "%d", avgDit);
    sprintf ( buf, "%s/%d", buf, avgDah);
    sprintf ( buf, "%s/%d", buf, avgDeadSpace );
    //    sprintf ( buf,"%s/%d",buf, ltrBrk);
  }
  //now, only update/refresh status area of display, if info has changed
  int ptr = 0;
  unsigned long chksum = 0;
  while ( buf[ptr] != 0) {
    chksum += buf[ptr];
    ++ptr;
  }
  if ( chksum != bfrchksum) {
    tft.setCursor(0, (row + 1) * (fontH + 10));
    tft.fillRect(0 , (row + 1) * (fontH + 10), 11 * fontW, fontH + 10, BLACK);
    //tft.fillRect(0 , (row+1)*(fontH+10), 14*fontW, fontH+10, BLACK);//tft.fillRect(0 , 8*(fontH+10), 10*fontW, fontH+10, BLACK);
    tft.print(buf);

  }
  bfrchksum = chksum;
  //  if(!mark) tft.print(buf);
  //  else{
  //     mark = false;
  //     tft.print("XXXXXX");
  //  }
}
