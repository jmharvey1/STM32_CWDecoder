/* REV: 2020-11-13 Yet another tweak to KeyEvntSR() to improve letter parsing with bug sent code*/ 
/* REV: 2020-10-28 Made small chnge to Timer_ISR(); added avgdeadspace test to enable shut down "glitch" buffer when interval drops below 36mS (mostly for cootie keys)   
/* REV: 2020-10-27 Minor change to KeyEvntSR() to improve "dit" & "dah" parsing withbug sent code*/
/* REV: 2020-10-23 More tweaks to CalcAvgPrd() to fix bug sent code from creating irrational speed and dah/dit ratios*/
/* REV: 2020-10-12 Changes to CalcAvgPrd() routine to improve response to external wpm speed changes */
/* REV: 2020-10-05 Minor changes to "CalcAvgPrd" routine to improve wpm speed correction response times */
/* REV: 2020-10-03 Tweaked Bug3 code (Key isr) to better diferentiate cadence changes to parse letter breaks. Also locked out the other two bug modes*/
/* REV: 2020-09-08 Greatly streamlined/simiplfied the code (installed yesterday) needed in Bug3 mode to recover key sequence data*/
/* REV: 2020-09-07 Added to BUG3 mode better management of "backspacing" display when correcting/updating DeCodeVal*/
/* Rev: 2020-08-27 Reworked "setup" button management, mainly to simplify & streamline the code needed to support them*/
/* Rev: 2020-08-25  Added "ScanFreq()"  to enable the tone detector to automatically "zero in" on the CW Audio tone (500 - 900 Hz) */
/* Rev: 2020-08-22  New "Bug1" Mode; uses "Key Down" event to manage/set "decodeval" value. */
/* Rev: 2020-08-14 Added sloppy sending checks (PD & DLL) */
/* Rev: 2020-08-12 Additional Tweaks to Maple Mini Sketch to improve decoding in a QRN environment*/
/* Rev: 2020-08-11 Added NSF (Noise Scale Factor) Parameter to list of User adjustable Settings*/
/* Rev: 2020-08-08 Added DeBuG Modes to list of User adjustable Settings*/
/* Rev: 2020-08-02 Added additional calls to EEPROM library, to store/retrieve user settings  plus a reset option to return user settings to original sketch values*/
/* Rev: 2020-07-22 Added calls to EEPROM library, to store and retrieve user settings (tone decode frequency)*/ 
/* Rev: 2020-06-18 main change added support for small(320X240) and large(480X320) screens*/
/* Rev: 2020-03-04 Same code as STM32_CWDecoderW_ToneDetectR02 but Changes to Squelch code to track noise floor to better capture tone signals being depressed by RCVR AGC changes */
/* Rev: 2020-02-22 Adjusted values to work with GY-MAX4466 Mic Board*/
/* Rev: 2020-01-29 1st attempt at porting Sketch to STM32 'Blue Pill' board*/
/*
     The TFTLCD library comes from here:  https://github.com/prenticedavid/MCUFRIEND_kbv
      note: after inporting the above library using the ide add zip library menu option, remane the folder to remover the word "master"
  ^   The Touch Screen & GFX libraries are standard (unmodified) Adafruit libraries & were found here:4
         https://github.com/adafruit/Adafruit-GFX-Library
         https://github.com/adafruit/Touch-Screen-Library
*/
char RevDate[9] = "20201113";
// MCU Friend TFT Display to STM32F pin connections
//LCD        pin |D7 |D6 |D5 |D4 |D3 |D2 |D1 |D0 | |RD  |WR |RS |CS |RST | |SD_SS|SD_DI|SD_DO|SD_SCK|
//Blue Pill  pin |PA7|PA6|PA5|PA4|PA3|PA2|PA1|PA0| |PB0 |PB6|PB7|PB8|PB9 | |PA15 |PB5  |PB4  |PB3   | **ALT-SPI1**
//Maple Mini pin |PA7|PA6|PA5|PA4|PA3|PA2|PA1|PA0| |PB10|PB6|PB7|PB5|PB11| |PA15 |PB5  |PB4  |PB3   | //Mini Mapl

#include "EEPROM.h"  //support storage & retrieval of user declared settings
#include "TouchScreen_kbv.h" //hacked version by david prentice
#include "Adafruit_GFX.h"
#include "gfxfont.h"
//#include <MCUFRIEND_kbv.h>
#include "MCUFRIEND_kbv.h"
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
/*
 * Rev: 2020-08-22:
 * This update debuts a new stategy in CW decoding. Its intended to better handle keying timing often found with "Bug" style sending. 
 * It differs from previous algorithms used in this sketch. Those all rely on timing linked to "key up" event. 
 * This method, instead, uses the "Key down" event to decide if what follows is a continuation of the last character sent, 
 * or truely a new charater. 
 * This stategy is enabled in the "Bug1" mode only.
 * 
 * At this time, its still, "a work in progress". 
 */
//#define BIAS 2000  //BluePill 1987 MapleMine 2000
int BIAS = 2000;  //BluePill 1987 MapleMine 2000
//#define DataOutPin PB5 //Sound process (Tone Detector) output pin; i.e. Key signal
#define DataOutPin PA9 //Sound process (Tone Detector) output pin; i.e. Key signal
// #define SlowData 6 // Manually Ground this Digital pin when when the incoming CW transmission is >30WPM
// When this pin is left open, slower code transmissions should work, & noise immunity will be better

// Timer2 interrupt stuff
#define SAMPL_RATE 94    // in microseconds; should give an interrupt rate of 10,526Hz (95)

#if defined(ARDUINO_MAPLE_MINI)
#define LED_BUILTIN PB1   //Maple Mini
#define MicPin PB0
#else
#define LED_BUILTIN PC13  //Blue Pill Pin
#define MicPin PB1
#endif
bool TonPltFlg = false;//used to control Arduino plotting (via) the USB serial port; To enable, don't modify here, but modify in the sketch
bool Test = false;//  true;//  //Controls Serial port printing in the decode character process; To Enable/Disable, modify here
/*
   Goertzel stuff
*/
#define FLOATING float

//#define TARGET_FREQUENCYC  749.5 //Hz
//#define TARGET_FREQUENCYL  734.0 //Hz
//#define TARGET_FREQUENCYH  766.0 //Hz
float TARGET_FREQUENCYC = 749.5; //Hz
float TARGET_FREQUENCYL = 734.0; //Hz
float TARGET_FREQUENCYH = 766.0; //Hz
float feqlratio = 0.979319546;
float feqhratio = 1.022014676;

#define CYCLE_CNT 6.0 // Number of Cycles to Sample per loop
float StrdFREQUENCY;// = float(ReadMemory(EEaddress));
float SAMPLING_RATE = 10850.0;// 10750.0;// 10520.0;//10126.5 ;//10520.0;//21900.0;//22000.0;//22800.0;//21750.0;//21990.0;
uint16 EEaddress = 0x10; // Start 2 words up from the base EEPROM page address (See EEPROM.H file for more detail)
int CurCnt = 0;
float AvgVal = 0.0;
float TSF = 1.2; //TSF = Tone Scale Factor; used as part of calc to set/determine Tone Signal to Noise ratio; Tone Component
float NSF = 1.0; //NSF = Noise Scale Factor; used as part of calc to set determine Tone Signal to Noise ratio; Noise Component   
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
float DimFctr = 1.00; // LED brightnes Scale factor
int kOld = 0;
// End Goertzel stuff

float ToneLvl = 0;
float noise = 0;
float SNR = 1.0;
long ManSqlch = 2000;
float AvgToneSqlch = 0;
int ToneOnCnt = 0;
int KeyState = 0;
int ltrCmplt = -1500;
bool badLtrBrk = false;
bool dletechar = false;
bool ConcatSymbl = false;
//bool newRow = false; //used to manage bug3 cursor position; especially critical when starting a new line during delete last characte

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
bool NoiseSqlch = true;
bool setupflg = false; // true while decoder is operating in "User Set Parameters" mode
/* Start Auto Tune varialbes */
bool AutoTune = true;
bool Scaning = false;
  
/* end Auto Tune variables */

/////////////////////////////////////////////////////////////////////
// CW Decoder Global Variables

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
//#if defined(ARDUINO_MAPLE_MINI)
//#define XP PB6
//#define YM PB7
//#else //BluePill Pins
//#define XP PB6
//#define YM PB7
//#endif
//copy and past the following two lines from report generated by running the STM32_TouchScreen_Calibr_native Sketch
const int XP=PB5,XM=PA7,YP=PA6,YM=PB6; //480x320 ID=0x9090
const int TS_LEFT=939,TS_RT=95,TS_TOP=906,TS_BOT=106; //

//const int XP=PB7,XM=PA1,YP=PA0,YM=PB6; //320x240 ID=0x9341
//const int TS_LEFT=91,TS_RT=881,TS_BOT=125,TS_TOP=883;

//const int XP=PB5,XM=PA7,YP=PA6,YM=PB6; //320x240 ID=0x4747
//const int TS_LEFT=84,TS_RT=891,TS_BOT=901,TS_TOP=121;


uint16_t ID = 0x9090; // Touch Screen ID obtained from either STM32_TouchScreen_Calibr_native or BluePillClock sketch
//uint16_t ID = 0x9341;
//uint16_t ID = 0x4747; 

//PORTRAIT CALIBRATION     320 x 480
//x = map(p.x, LEFT=115, RT=885, 0, 320)
//y = map(p.y, TOP=61, BOT=928, 0, 480)
//Touch Pin Wiring XP=PB6 XM=PA6 YP=PA7 YM=PB5
//LANDSCAPE CALIBRATION    480 x 320
//x = map(p.y, LEFT=61, RT=928, 0, 480)
//y = map(p.x, TOP=885, BOT=115, 0, 320)
//px = map(tp.x, TS_LEFT, TS_RT, 0, scrnWidth);
//py = map(tp.y, TS_TOP, TS_BOT, 0, scrnHeight);
    
 
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
int CPL = 40; //number of characters each line that a 3.5" screen can contain 

int curRow = 0; 
int offset = 0; 


bool newLineFlag = false;
char Pgbuf[448];
char Msgbuf[50];
char Title[50];
char P[13];
int statsMode = 0;
bool SCD = true; //false;//true; //Sloppy Code Debugging; if "true", use ide Serial Monitor to see output/results
bool FrstSymbl = false;
bool chkStatsMode = true;
bool SwMode = false;
bool BugMode = false;//true;//
bool Bug2 = false;//false;//
bool Bug3 = false;
bool NrmMode = true;
bool CalcAvgdeadSpace = false;
bool CalcDitFlg = false;
bool LtrBrkFlg = true;
bool NuWrd = true;
bool NuWrdflg = false;
bool SetRtnflg = false;
unsigned long AvgShrtBrk = 0;
unsigned long AvgShrtBrk1 = 0;
int unsigned ClrBtnCnt = 0;
int unsigned ModeBtnCnt = 0; 
int unsigned ModeCnt = 0; // used to cycle thru the 4 decode modes (norm, bug1, bug2, bug3)
float LtBrkSF = 0.3; //leter Break Scale factor. Used to catch sloppy sending; typical values //(0.66*ltrBrk)  //(0.15*ltrBrk) //(0.5*ltrBrk)
int unsigned ModeCntRef = 0;
//char TxtMsg1[] = {"1 2 3 4 5 6 7 8 9 10 11\n"};
//char TxtMsg2[] = {"This is my 3rd Message\n"};
int msgcntr = 0;
int badCodeCnt = 0;
int dahcnt =0;
int MsgChrCnt[2];
char newLine = '\n';

//volatile bool state = LOW;
volatile bool valid = LOW;
volatile bool mark = LOW;
bool dataRdy = LOW;

volatile long period = 0;
volatile unsigned long STart = 0;
volatile unsigned long Oldstart = 0;
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
volatile unsigned int DCVStrd[2];//used by Bug3 to recover last recored DeCodeVal;

volatile unsigned int OldDeCodeVal;
long avgDit = 80; //average 'Dit' duration
long DitVar = 0; //average 'Dit' duration
unsigned long avgDah = 240; //average 'Dah' duration
//float avgDit = 80; //average 'Dit' duration
//float avgDah = 240; //average 'Dah' duration
float lastDit = avgDit;

int badKeyEvnt = 0;
volatile unsigned long avgDeadSpace = avgDit;
volatile unsigned long BadDedSpce = 0;
volatile int space = 0;
unsigned long lastDah = avgDah;
volatile unsigned long letterBrk = 0; //letterBrk =avgDit;
volatile unsigned long letterBrk1 = 0;
volatile unsigned long OldltrBrk = 0;
unsigned long ShrtBrkA = 0;
unsigned long UsrLtrBrk = 100; // used to track the average letter break interval
float ShrtFctr = 0.48;//0.52;// 0.45; //used in Bug3 mode to control what precentage of the current "UsrLtrBrk" interval to use to detect the continuation of the previous character
//unsigned long ShrtBrk0 = 0;
//unsigned long ShrtBrk[10];
int ShrtBrk[10];
int LtrCntr =0;
int OldLtrPntr =0;
//unsigned long ShrtBrk2 = 0;
unsigned long AvgLtrBrk = 0;
volatile unsigned long ltrBrk = 0;
int shwltrBrk = 0;
volatile unsigned long wordBrk = avgDah ;
volatile unsigned long wordStrt;
volatile unsigned long deadTime;
unsigned long deadSpace = 0;
unsigned long LastdeadSpace = 0;
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
int DeBug = 0;
int BadDedSpceCnt =0;
//unsigned long AvgBadDedSpce =0;
unsigned long BadSpceStk[5];
//int BPtr = 0;
char DeBugMsg[150];

struct DF_t { float DimFctr; float TSF; int BIAS; long ManSqlch; bool NoiseSqlch; int DeBug; float NSF; bool AutoTune; float TARGET_FREQUENCYC;};
struct BtnParams {
   int BtnXpos;  //Button X position
   int BtnWdth;  //Button Width
   int BtnYpos;  //Button X position
   int BtnHght;  //Button Height
   const char* Captn;
   unsigned int BtnClr; 
   unsigned int TxtClr;
};

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
#define ARSIZE2 124
static unsigned int CodeVal2[ARSIZE2]={
  19,
  21,
  28,
  31,
  34,
  38,
  40,
  41,
  42,
  44,
  45,
  46,
  52,
  54,
  55,
  57,
  58,
  59,
  61,
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
  88,
  89,
  90,
  91,
  92,
  96,
  105,
  106,
  110,
  113,
  114,
  116,
  118,
  120,
  121,
  122,
  123,
  124,
  125,
  126,
  127,
  138,
  145,
  146,
  148,
  150,
  156,
  162,
  176,
  178,
  180,
  185,
  191,
  202,
  209,
  211,
  212,
  213,
  216,
  218,
  232,
  234,
  240,
  241,
  242,
  243,
  244,
  246,
  248,
  249,
  251,
  283,
  296,
  324,
  328,
  360,
  362,
  364,
  416,
  429,
  442,
  443,
  468,
  482,
  486,
  492,
  493,
  494,
  500,
  502,
  510,
  596,
  708,
  716,
  731,
  790,
  832,
  842,
  862,
  899,
  922,
  968,
  970,
  974,
  1348,
  1451,
  1480,
  1785,
  1795,
  1940,
  1942,
  6134,
  6580,
  14752
};

char DicTbl2[ARSIZE2][5]={
  "UT",
  "<AA>",
  "GE",
  "TO",
  "VE",
  "UN",
  "<AS>",
  "AU",
  "<AR>",
  "AD",
  "WA",
  "AG",
  "ND",
  "<KN>",
  "NO",
  "MU",
  "GN",
  "TY",
  "OA",
  "HI",
  "<SK>",
  "SG",
  "US",
  "UR",
  "UG",
  "RS",
  "AV",
  "AF",
  "AL",
  "86",
  "AB",
  "WU",
  "AC",
  "AY",
  "92",
  "THE",
  "CA",
  "TAR",
  "TAG",
  "GU",
  "GR",
  "MAI",
  "MP",
  "OS",
  "OU",
  "OR",
  "MY",
  "OD",
  "MY",
  "OME",
  "TOO",
  "VR",
  "FU",
  "UF",
  "UL",
  "UP",
  "UZ",
  "AVE",
  "WH",
  "PR",
  "AND",
  "WX",
  "JO",
  "TUR",
  "CU",
  "CW",
  "CD",
  "CK",
  "YS",
  "YR",
  "QS",  
  "QR",
  "OH",
  "OV",
  "OF",
  "OUT",
  "OL",
  "OP",
  "OB",
  "OX",
  "OY",
  "VY",
  "FB",
  "LL",
  "RRI",
  "WAS",
  "WAR",
  "AYI",
  "CH",
  "CQ",
  "NOR",
  "NOW",
  "MAL",
  "OVE",
  "OUN",
  "OAD",
  "OAK",
  "OWN",
  "OKI",
  "OYE",
  "OON",
  "UAL",
  "WIL",
  "W?",
  "WAY",
  "NING",
  "CHE",
  "CAR",
  "CON",
  "<73>",
  "MUC",
  "OUS",
  "OUR",
  "OUG",
  "ALL",
  "WARM",
  "JUS",
  "YOU",
  "73",
  "OUL",
  "OUP",
  "JOYE",
  "XYL",
  "MUCH"
};


//void Timer_ISR(void);
// End of CW Decoder Global Variables

struct DF_t DFault;
////////////////////////////////////////////////////////////////////
// Special handler functions to store and retieve non-volital user data
template <class T> int EEPROM_write(uint ee, const T& value)
{
   const byte* p = (const byte*)(const void*)&value;
   int i;
   for (i = 0; i < sizeof(value); i++)
       EEPROM.write(ee++, *p++);
   return i;
}

template <class T> int EEPROM_read(int ee, T& value)
{
   byte* p = (byte*)(void*)&value;
   int i;
   for (i = 0; i < sizeof(value); i++)
       *p++ = EEPROM.read(ee++);
   return i;
}
////////////////////////////////////////////////////////////////////////////
void SetDBgCptn(char *textMsg){
  //char textMsg[15];
  switch(DeBug){
    case 0:
      TonPltFlg = false;
      Test = false;
      sprintf(textMsg, " DEBUG Off");
      break;  
    case 1:
      
      Test = false;
      sprintf(textMsg, " DEBUG Plot");
      // Graph Geortzel magnitudes
      Serial.print("FrqH");
      //Serial.print(magH);//Blue
      Serial.print("\t");
      Serial.print("FrqL");
      //Serial.print(magL);//Red
      Serial.print("\t");
      Serial.print("FrqC");
      //Serial.print(magC);//Green
      Serial.print("\t");
  
      // Control Sigs
      Serial.print("ToneLvl");//Serial.print(mag);//Serial.print(ToneLvl);//Orange
      Serial.print("\t");
      Serial.print("Noise");//Purple
      Serial.print("\t");
      Serial.print("SqlchVal");//Gray//Serial.print(AvgToneSqlch);//Gray   
      Serial.print("\t");
      Serial.print("KeyState");//Light Blue
      Serial.print("\t");
      Serial.print("LtrCmplt");//Black
      Serial.print("\t");
      Serial.println("BadLtrBrk");//BLue
      TonPltFlg = true;
      break;
    case 2:
      TonPltFlg = false;
      Test = true;
      sprintf(textMsg, "DEBUG Decode");
      break;
    default:
      sprintf(textMsg, "DB Val: %d", DeBug);
      break;
        
  }
}

///////////////////////////////////////////////////////////////////////////


// Install the interrupt routine.
void KeyEvntSR() {
  ChkDeadSpace();
  SetLtrBrk();
  chkChrCmplt();
  if (digitalRead(interruptPin) == LOW) { //key-down
    Oldstart = STart;
    STart = millis();
    deadSpace = STart - noSigStrt;
    OldltrBrk = letterBrk;
    // 20200818 jmh following 3 lines to support sloppy code debuging
    ShrtBrk[0] = int(STart - letterBrk1);
    //Serial.println(ShrtBrk[0]);
// test to detect sloppy sender timing, if "true" this keydown event appears to be a continuation of previous character
    //if( (ShrtBrk[0] < ShrtBrkA) && Bug3 && (cnt>CPL+1)){
    if( (ShrtBrk[0] < ShrtBrkA) && Bug3){ // think I fixed needing to wait until we're on the 2nd line of the display "&& (cnt>CPL+1)"
      badLtrBrk = true; // this flag is just used to drive the "toneplot" graph
      //if(ShrtBrk[0] < 1.1*AvgShrtBrk && (OldLtrPntr ==1)){// added this if condition to detect subtle changes in cadence
     AvgShrtBrk = (56*ltrBrk)/100;//AvgShrtBrk = (79*ltrBrk)/100;
     if((ShrtBrk[0] < AvgShrtBrk) || (OldLtrPntr == 1)){
        //AvgShrtBrk1 = (5*AvgShrtBrk1+ShrtBrk[0])/6;
        if(Bug3 && SCD && Test){
           Serial.print("Concatenate  ");
           Serial.print(OldLtrPntr);
           Serial.print(" "); 
           for( int i =0; i <= OldLtrPntr; i++){
            Serial.print(ShrtBrk[i]);
            Serial.print(";"); 
           }
           //Serial.print(ShrtBrk[0]);
           Serial.print("  ");
           Serial.println(AvgShrtBrk);
         }
         //AvgShrtBrk = (3*AvgShrtBrk+ShrtBrk[0])/4;// update AvgShrtBrk
         if(ShrtBrk[0]> UsrLtrBrk/5) AvgShrtBrk = (2*AvgShrtBrk+ShrtBrk[0])/3;// update AvgShrtBrk
           
        //test to make sure that the last letter received has actually been processed 
        int BPtr = 0;
        while(CodeValBuf[BPtr] != 0) {
          DeCodeVal = CodeValBuf[BPtr];
  //        Serial.print(BPtr);
  //        Serial.print('\t');
  //        Serial.print(CodeValBuf[BPtr]);
  //        Serial.print('\t');
          ++BPtr;
        }
        if(BPtr>0) CodeValBuf[BPtr] = 99999;
        if(DeCodeVal == 0){//Last letter received was posted to the screen
          //go get that last character displayed
  //        Serial.print("cnt: ");
  //        Serial.println(cnt);
  //        if(cnt>CPL+1){//Pgbuf now has enough data to recover sloppy sending results
            if(MsgChrCnt[1] >0){
              DeCodeVal = DCVStrd[1];
              dletechar = true;
              FrstSymbl = true;
              ConcatSymbl = true;// used to verify (at the next letter break) we did the right thing;
            }
          else badLtrBrk = false;
        }
        else{ //abort letter break process
          if(Bug3 && SCD && Test){
           Serial.print("Clawed Last Letter Back");
  //         Serial.print('\t');
  //         Serial.print(BPtr);
           Serial.print('\t');
           Serial.println(DeCodeVal);
          } 
        }
      }
      else{
        if(Bug3 && SCD && Test){
          Serial.print("Skip Concat  ");
          Serial.print(OldLtrPntr);
          Serial.print(" "); 
          for( int i =0; i <= OldLtrPntr; i++){
           Serial.print(ShrtBrk[i]);
           Serial.print(";"); 
          }
          //Serial.print(ShrtBrk[0]);
          Serial.print("  ");
          Serial.println(AvgShrtBrk);
        }
        AvgShrtBrk = (AvgShrtBrk+ShrtBrk[0])/2;// update AvgShrtBrk
        //AvgShrtBrk = (2*AvgShrtBrk+ShrtBrk[0])/3;// update AvgShrtBrk
        //AvgShrtBrk = (4*AvgShrtBrk+ShrtBrk[0])/5;// update AvgShrtBrk
      }
    }


    
    letterBrk = 0;
    ltrCmplt = -1800; // used in plot mode, to show where/when letter breaks are detected 
    CalcAvgdeadSpace = true;
    
    if (wordBrkFlg) {
      wordBrkFlg = false;
      thisWordBrk = STart - wordStrt;
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
      unsigned long OldNoStrt = noSigStrt;
      badLtrBrk = false;
      noSigStrt =  millis();
      //noSigStrt =  micros();
      period = noSigStrt - STart;
      if(2*period<avgDit){ //seems to be a glitch
        if(DeCodeVal == 1) return;// its a glitch before any real key closures have been detected, so ignore completely 
        if(magC>10000) avgDit= (5*avgDit +period)/6; //go ahead & factor this period in, just incase we are now listening to a faster WPM stream
        noSigStrt = OldNoStrt;
        STart = Oldstart;
        letterBrk = OldltrBrk;
        return;// abort processing this event 
      }
      WrdStrt = noSigStrt;
      TimeDat[Bitpos] = period;
      Bitpos += 1;
      if (Bitpos > 15) {
        Bitpos = 15; // watch out: this character has too many symbols for a valid CW character
        letterBrk = noSigStrt; //force a letter brk (to clear this garbage
        return;
      }
     
      STart = 0;
    } //End if(DeCodeVal!= 0)

  }// end of key interrupt processing;
  // Now, if we are here; the interrupt was a "Key-Up" event. Now its time to decide whether this last "Key-Down" period represents a "dit", a "dah", or just garbage.
  // 1st check. & throw out key-up events that have durations that represent speeds of less than 5WPM.
  if (period > 720) { //Reset, and wait for the next key closure
    //period = 0;
    noSigStrt =  millis();//jmh20190717added to prevent an absurd value
    DeCodeVal = 0;
    dletechar = false;
    FrstSymbl = false;
    ConcatSymbl = false;
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
    //if ((period >= 1.4 * avgDit)&& Bug3) NrmlDah = true; // if in bug mode we'll let a slightly shorter key closer count as a "Dah". 
    //else if ((period >= (1.4*avgDit)+(2*DitVar))&& Bug3){  //JMH 20201002 if in bug mode we'll let a slightly shorter key closer count as a "Dah".
    else if ((period >= (1.46*avgDit)+(DitVar))&& Bug3){
      NrmlDah = true;
      Serial.print("DitVar: ");
      Serial.println(DitVar); 
    }
    bool NrmlDit = false;
    //if ((period < 1.3 * avgDit)|| (period < 0.4 * avgDah)) NrmlDit = true;
    //if (((period >= 1.8 * avgDit)|| (period >= 0.8 * avgDah))||(DeCodeVal ==2 & period >= 1.5 * lastDit)) { // it smells like a "Dah".
    //if ((NrmlDah)||((DeCodeVal ==2) & (period > 1.46 * avgDit)&& (Bug2 || Bug3))) { // it smells like a "Dah".
     if ((NrmlDah)||((DeCodeVal ==2) & ((period-10) > 1.4 * avgDit)&& (Bug2 || Bug3))) { // it smells like a "Dah".  think there's 10 millisecond uncertainty due to the sampling speed, sowe're going to use the smallest possible interval for this decision
     
       if((MsgChrCnt[1] >0) && (deadSpace < 2.76*avgDeadSpace)&& (deadSpace > 1.4* avgDeadSpace) && !NuWrd && !dletechar && Bug3){
       //if(0){
        DeCodeVal = DCVStrd[1];
        DeCodeVal = DeCodeVal << 1;
        DeCodeVal = DeCodeVal + 1;
        dletechar = true;
        FrstSymbl = false;//FrstSymbl = true;
        ConcatSymbl = true;// used to verify (at the next letter break) we did the right thing;
//        Serial.print("\tXX\t");
//        Serial.print(deadSpace);
//        Serial.print('\t');
//        Serial.print(avgDeadSpace);
//        Serial.print('\t');
//        Serial.print(DCVStrd[1]);
//        Serial.print('\t');
//        Serial.println(DeCodeVal); 
      }
      else{
        //JMH added 20020206
        DeCodeVal = DeCodeVal + 1; // it appears to be a "dah' so set the least significant bit to "one"
//        Serial.print("\tYY\t");
//        Serial.print(deadSpace);
//        Serial.print('\t');
//        Serial.print(avgDeadSpace);
//        Serial.print('\t');
//        Serial.print(DCVStrd[1]);
//        Serial.print('\t');
//         Serial.print(DCVStrd[0]);
//        Serial.print('\t');
//        Serial.print(DeCodeVal);
//        Serial.print("\tdletechar = ");
//        if(dletechar)Serial.print("true;  ");
//        else Serial.print("false;  ");
//        Serial.print("\ConcatSymbl = ");
//        if(ConcatSymbl)Serial.print("true");
//        else Serial.print("false");
//        Serial.println(""); 
      }
      //if(Bug3 & SCD& badLtrBrk) sprintf(DeBugMsg, "1%s", DeBugMsg);
      if(Bug3 && SCD && Test){
        //sprintf(DeBugMsg, "1%s", DeBugMsg);
        int i =0;
        while(DeBugMsg[i] !=0) i++;
        DeBugMsg[i] = '1';
      }
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
      //if(Bug3 & SCD& badLtrBrk) sprintf(DeBugMsg, "0%s", DeBugMsg);
      if(Bug3 && SCD && Test){
        //sprintf(DeBugMsg, "0%s", DeBugMsg);
        int i =0;
        while(DeBugMsg[i] !=0)i++;
        DeBugMsg[i] = '0';
      }
      //if(FrstSymbl && ((DeCodeVal & 2) == 0)){// FrstSymbl can only be true if we are in Bug3 mode; if (DeCodeVal & 2) == 0, then the last symbol of the preceeding letter was a 'dit'
      if(FrstSymbl){ //JMH 20201003 New way, doesn't make any difference about the last symbol of the preceeding letter. The first symbol in the current letter is a 'dit', so forget the past
        //if we're here then we have recieved 2 'dits' back to back with a large gap between. So assume this is the begining of a new letter/character
        //put everything back to decoding a 'normal' character
        DeCodeVal = DeCodeVal>>1;
//        Serial.print(DeCodeVal);
//        Serial.print('\t');
//        Serial.println( DicTbl1[linearSearchBreak(DeCodeVal, CodeVal1, ARSIZE)] );
        DeCodeVal = 2; 
        dletechar = false;
        FrstSymbl = false;
        ConcatSymbl = false;
//        Serial.println("RESET");
      }
      lastDit = period;
      dahcnt = 0;
      if (DeCodeVal != 2 ) { // don't recalculate speed based on a single dit (it could have been noise)or a single dah ||(curRatio > 5.0 & period> avgDit )
        CalcDitFlg = true;
        DitVar = ((7*DitVar)+(abs(avgDit-period)))/8;
//        Serial.print("DitVar: ");
//        Serial.println(DitVar);
      }
    }
    FrstSymbl = false;
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
    int k = analogRead(MicPin);    // read the ADC value from pin PB1; (k = 0 to 4096)
    k -= BIAS; // form into a signed int
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
  AvgVal = NSF* AvgVal;
  bool GudTone = true;
  float CurNoise = ((AvgVal) - (TSF*mag)); //((CurNoise) + 2.0*((AvgVal) - (TSF*mag))) / 2;
  ToneLvl = mag;
  noise = ((4 * noise) + (CurNoise)) / 5; // calculate the running average of the unfiltered digitized Sound
  if (NoiseSqlch){ 
    if (mag < 2000) GudTone = false; //JMH20200128
    if (ToneLvl > noise & ToneLvl >  CurNoise) {
      ++ToneOnCnt;
      if (ToneOnCnt >= 4) {
        ToneOnCnt = 3;
        MidPt =  ((2 * MidPt) + (((ToneLvl - NoiseAvgLvl) / 4) + NoiseAvgLvl)) / 3;
        if (MidPt >  AvgToneSqlch) AvgToneSqlch = MidPt;
      }
    }
    //SqlchVal = noise+(0.5*((AvgVal) - (TSF*mag))); //JMH 20200809 added "+(0.5*((AvgVal) - (TSF*mag)))" to better supress lightening noise
    SqlchVal = noise+(0.5*CurNoise);
    SqlchVal = noise+CurNoise;
    if (AvgToneSqlch > SqlchVal) SqlchVal = AvgToneSqlch;
    //if(Scaning) SqlchVal += SqlchVal;
  }
  else{
    SqlchVal = ManSqlch;
  }
  
  if ( ToneLvl > SqlchVal) {
    magC = (14 * magC + mag) / 15;
    magL = (14 * magL + (sqrt(GetMagnitudeSquared(Q1, Q2, coeffL)))) / 15;
    magH = (14 * magH + (sqrt(GetMagnitudeSquared(Q1H, Q2H, coeffH)))) / 15;
    SNR = ((5*SNR)+(ToneLvl / CurNoise))/6;// SNR = ToneLvl / SqlchVal;//
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
    //Serial.print(1.0*((AvgVal) - (TSF*mag)));
    Serial.print("\t");
    Serial.print(SqlchVal);//Gray//Serial.print(AvgToneSqlch);//Gray   
    Serial.print("\t");
  }
  
  if (( ToneLvl > SqlchVal)& !Scaning) {
  //if (( ToneLvl > noise)) {  
    toneDetect = true; 
  } else toneDetect = false;

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
  //20201028 added "avgDeadSpace>35" condition - primarilly for "Cootie" Ops which seem to use unusually short dead space intervals; Should/could also help wid High speed decoding
  if (avgDeadSpace>35) { //if(digitalRead(SlowData)){
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
    Serial.print(ltrCmplt);//Black
    Serial.print("\t");
    float badltrbrkT = -4000;
    if(badLtrBrk) badltrbrkT = -1900;
    Serial.println(badltrbrkT);//BLue
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
  LEDGREEN = (byte)(DimFctr*((float)LEDGREEN));
  LEDRED = (byte)(DimFctr*((float)LEDRED));
  LEDBLUE = (byte)(DimFctr*((float)LEDBLUE));
  strip.setPixelColor(0, strip.Color(LEDGREEN, LEDRED, LEDBLUE));
  strip.show(); //activate the RGB LED
  // this round of sound processing is complete, so setup to start to collect the next set of samples
  if(AutoTune) ScanFreq(); // go check to see if the center frequency needs to be adjusted
  ResetGoertzel();
  AvgVal = 0.0;
  OvrLd = false;
  CurCnt = 0;
} //End Timer2 ISR
///////////////////////////////////////////////////////////////////////////////////////////////////////
void ScanFreq(void){ 
  /*  this routine will start a Sweep the audio tone range by decrementing the Geortzel center frequency when a valid tome has not been heard 
   *  withn 4 wordbreak intervals since the last usable tone was detected
   *  if nothing is found at the bottom frquency (500Hz), it jumps back to the top (900 Hz) and starts over
  */
  float DltaFreq = 0.0;
  //if(setupflg) return;
//  Serial.print(TARGET_FREQUENCYC);
//  Serial.print('\t');
//  Serial.println(SqlchVal);
  if( ToneLvl > SqlchVal){ //We have a valid tone.
    Scaning = false;  //enable the tonedetect process to allow key closer events.
    if(magC> magL & magC> magH) return;
    if(magH> magL) DltaFreq = +2.5;
    else DltaFreq = -2.5;
  } else {
    if((millis()-noSigStrt) > 5*wordBrk){
       DltaFreq = -20.0;
      Scaning = true;  //lockout the tonedetect process while we move to a new frequency; to prevent false key closer events while in the frequency hunt mode
    }
  }
  if(DltaFreq == 0.0) return;//Go back; Nothing needs fixing
  TARGET_FREQUENCYC += DltaFreq;
  if(TARGET_FREQUENCYC < 500) TARGET_FREQUENCYC =900.0;// start over( back to the top frequency); reached bottom of the frequency range 
  CalcFrqParams(TARGET_FREQUENCYC);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  //delay(5000);
  //setup Digital signal output pin
  pinMode(DataOutPin, OUTPUT);
  digitalWrite(DataOutPin, HIGH);
  pinMode(MicPin, INPUT);
  Serial.begin(115200); // use the serial port
  strip.begin();
  //store default settings for later recovery
  DFault.DimFctr = DimFctr;
  DFault.TSF = TSF;
  DFault.NSF = NSF; 
  DFault.BIAS = BIAS; 
  DFault.ManSqlch = ManSqlch; 
  DFault.NoiseSqlch = NoiseSqlch;
  DFault.DeBug = DeBug;
  DFault.AutoTune = AutoTune;
  DFault.TARGET_FREQUENCYC = TARGET_FREQUENCYC;
  int n= EEPROM_read(EEaddress+0, StrdFREQUENCY);
  if(StrdFREQUENCY !=0 && n == 4){
    TARGET_FREQUENCYC = StrdFREQUENCY; //Hz
    TARGET_FREQUENCYL = feqlratio*StrdFREQUENCY; //Hz
    TARGET_FREQUENCYH = feqhratio*StrdFREQUENCY; //Hz
  }
  float StrdVal = 0;//  float StrdDimFctr = 0;
  int StrdintVal = 0;
  long StrdLngVal = 0;
  bool StrdTFVal;
  n = EEPROM_read(EEaddress+4, StrdVal);
  if(StrdVal > 0 && n == 4){
    DimFctr = StrdVal;
  }
  StrdVal = 0;
  n = EEPROM_read(EEaddress+8, StrdVal);
  if(StrdVal > 0 && n == 4){
    TSF = StrdVal;
  }
  n = EEPROM_read(EEaddress+12, StrdintVal);
  if(StrdVal > 0 && n == 4){
    BIAS = StrdintVal;
  }

  n = EEPROM_read(EEaddress+16, StrdLngVal);
  if(StrdLngVal > 0 && n == 4){
    ManSqlch = StrdLngVal;
  }
  
  n = EEPROM_read(EEaddress+20, StrdTFVal);
  if(n == 1){
    NoiseSqlch = StrdTFVal;
  }
  
  n = EEPROM_read(EEaddress+24, StrdintVal);
  if(StrdintVal > 0 && n == 4){
    DeBug = StrdintVal;
  }

  n = EEPROM_read(EEaddress+28, StrdVal);
  if(StrdVal > 0 && n == 4){
    NSF = StrdVal;
  }

  n = EEPROM_read(EEaddress+32, StrdTFVal);
  if( n == 1){
    byte tstval = byte(StrdTFVal)&1;// inserted this and the next line in an attempt to flush out a bad stored value
    StrdTFVal = bool(tstval);
//    Serial.println(StrdTFVal);
    AutoTune = StrdTFVal;
    
  }
  
  InitGoertzel();
  coeff = coeffC;
  N = NC;

  switch (DeBug) {
    case 0:
      TonPltFlg = false;// false;//used to control Arduino plotting (via) the USB serial port; To enable, don't modify here, but modify in the sketch
      Test = false;//  true;//  //Controls Serial port printing in the decode character process; To Enable/Disable, modify here
      break;
    case 1:
      TonPltFlg = true;
      Test = false;
      break;
    case 2:
      TonPltFlg = false;
      Test = true;
      break;  
  }

  //Setup & Enable Timer ISR (in this case Timer2)
  Timer2.setMode(TIMER_CH1, TIMER_OUTPUTCOMPARE);
  Timer2.setPeriod(SAMPL_RATE); // in microseconds
  Timer2.setCompare(TIMER_CH1, 1); // overflow might be small
  Timer2.attachInterrupt(TIMER_CH1, Timer_ISR);
  // End of Goertzel Tone Processing setup
  //Begin CW Decoder setup
  tft.reset();
  //  ID= 0x9486;// MCUfriends.com 3.5" Display id;
  //  ID = 0x9341;// MCUfriends.com 2.4" Display id; (White Letters on Black Screen)
  //  ID = 0x6814;// MCUfriends.com 3.5" Display id
  //  ID = 0x9090;//MCUfriends.com 3.5" Display id (Black Letters on White Screen)
  //  ID = 0x4747;//MCUfriends.com 2.8" Display id
  if (ID == 0x9090) ID = 0x9486; //do this to fix color issues 
  tft.begin(ID);//  The value here is screen specific & depends on the chipset used to drive the screen,
  tft.setRotation(1); // valid values 1 or 3
  tft.fillScreen(BLACK);
  scrnHeight = tft.height();
  scrnWidth = tft.width();
  displayW = scrnWidth;
  //Serial.print("scrnHeight: ");
  //Serial.println(scrnHeight);
  // PORTRAIT is 240 x 320 2.8" inch Screen
  if (scrnHeight == 240) { //we are using a 3.5 inch screen
    px = 0; //mapped 'x' display touch value
    py = 0; //mapped 'y' display touch value
    LineLen = 27; //max number of characters to be displayed on one line
    row = 7;//7; //max number of lines to be displayed
    ylo = 200; //button touch reading 2.8" screen
    yhi = 250; //button touch reading 2.8" screen
    Stxl = 21; //Status touch reading 2.8" screen
    Stxh = 130; //Status touch reading 2.8" screen
    bxlo = 130; //blue button (CLEAR) touch reading 2.8" screen
    bxhi = 200; //blue button (CLEAR)touch reading 2.8" screen
    gxlo  = 200; //green button(MODE) touch reading 2.8" screen
    gxhi  = 290; //green touch (MODE)reading 2.5" screen
    CPL = 27; //number of characters each line that a 2.8" screen can contain 

    
//row = 10; //max number of decoded text rows that can be displayed
//ylo = 0; //button touch reading 3.5" screen
//yhi = 40; //button touch reading 3.5" screen
//    Stxl = 0; //Status touch reading 3.5" screen
//    Stxh = 150; //Status touch reading 3.5" screen
//    bxlo = 155; //button touch reading 3.5" screen
//    bxhi = 236; //button touch reading 3.5" screen
//    gxlo  = 238; //button touch reading 3.5" screen
//    gxhi  = 325; //button touch reading 3.5" screen
   }
   
   MsgChrCnt[0] = 0;

  DrawButton();
  Button2();
  WPMdefault();
  tft.setCursor(textSrtX, textSrtY);
  tft.setTextColor(WHITE);//tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(2);
  tft.setTextWrap(false);

  enableINT(); //This is a local function, and is defined below

  STart = 0;
  WrdStrt = millis();
//  Serial.print("Starting...");
//  sprintf( Title, "             KW4KD (%s)           ", RevDate );
  if (scrnHeight == 320) { //we are using a 3.5 inch screen
    sprintf( Title, "             KW4KD (%s)           ", RevDate );
    
  }
  else {
    sprintf( Title, "      KW4KD (%s)     ", RevDate );
    
  }
  dispMsg(Title);
  wordBrk = ((5 * wordBrk) + 4 * avgDeadSpace) / 6;
  sprintf(DeBugMsg, "");
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
  tchcnt -=1; //decrement "techcnt" by 1
  if(tchcnt==0){
    readResistiveTouch();
  } else tp = {0, 0, 0};
  //if(!ScrnPrssd & btnPrsdCnt > 0) btnPrsdCnt = 0;  
  if (tp.z > 200) { // if (tp.z > MINPRESSURE && tp.z < MAXPRESSURE) { //
    //use the following for Screen orientation set to 1
    py = map(tp.y, TS_TOP, TS_BOT, 0, scrnHeight);
    px = map(tp.x, TS_LEFT, TS_RT, 0, scrnWidth);

    
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
//          if (statsMode == 0) {
//            statsMode = 1;
//            //Serial.println("SET statsMode =1");
//          } else {
//            statsMode = 0;
//            //Serial.println("SET statsMode =0");
//          }
          statsMode++;
          if(statsMode >3) statsMode = 0;
          showSpeed();  
        }
        else btnPrsdCnt = 10;
      }

    }
  }//end tft display "touch" check

    if (px > bxlo && px < bxhi && py > ylo && py < yhi && buttonEnabled) // The user has touched a point inside the Blue rectangle, & wants to clear the Display
    { //reset (clear) the Screen
      SetRtnflg = false;
      SftReset();

    } else if (px > bxlo && px < bxhi && py > ylo && py < yhi && !buttonEnabled && !SetRtnflg) // The user has touched a point inside the Blue rectangle, but the screen has already been cleared.
    // so need to test if user presses long engough to signal change to go to menu setup mode
    {
      ClrBtnCnt += 1;
      if(ClrBtnCnt == 10000){ //the user has pressed the clear button long enough to go into "setup mode" 
       setuploop(); 
      }
    } else if (px > gxlo && px < gxhi && py > ylo && py < yhi && buttonEnabled) { //User has touched a point inside the Green Mode rectangle, & wants to change Mode (normal/bug1/bug2)
      px = 0; //kill button press
      py = 0; //kill button press
      //Serial.println("Mode Button");
      //buttonEnabled = false;
      if (btnPrsdCnt < 10) { // press screen 'debounce' interval
        btnPrsdCnt += 1;
        //Serial.println(btnPrsdCnt);
      }
      else { // user pressed screen long enough to indicate real desire to change Mode (normal/bug1/bug2/bug3)
        if(btnPrsdCnt == 10){
          buttonEnabled = false;
          ModeCnt += 1;
          if (ModeCnt == 2) ModeCnt = 0;//if (ModeCnt == 4) ModeCnt = 0;
          //ModeCntRef = ModeCnt;
          SetModFlgs(ModeCnt);
          enableDisplay();
          Button2();
        }
        else btnPrsdCnt = 10;
      }
    }
  chkChrCmplt();
  while (CodeValBuf[0] > 0) {
    DisplayChar(CodeValBuf[0]);
  }
  SetModFlgs(ModeCnt);//if(ModeCntRef != ModeCnt) ModeCnt= ModeCntRef; //added this in an attempt to fix mysterious loss of value in ModeCnt

}//end of Main Loop
//////////////////////////////////////////////////////////////////////////

void setuploop(){
  bool saveflg = false;
  bool NoSigFlg = false;
  buttonEnabled = true;
  int loopcnt = 100;
  int LEDLpcnt = 1;
  float StrdDimFctr = 0;
  int btnHght = 40;
  int btnWdthS = 80;
  int btnWdthL = 2*btnWdthS;
  int row0 = scrnHeight - (btnHght + 5);
  int row1 = scrnHeight - (2*btnHght + 5);
  int row2 = scrnHeight - (3*btnHght + 5);
  int row3 = scrnHeight - (4*btnHght + 5);
  int row4 = scrnHeight - (5*btnHght + 5);
  int col0 = 0;
  int col1 = btnWdthS+1;
  int col2 = col1 + btnWdthS+1;
  int col3 = col2+ btnWdthS+1;
  int col4 = col3+ btnWdthS+1;
  int col5 = col4+ btnWdthS+1;
  char BtnCaptn[14];
  char BtnCaptn1[14];
  char BtnCaptn2[14];
  bool CurSCD = SCD;
  SCD = false;
  BtnParams SetUpBtns[18]; //currently the Setup screen has 18 buttons
 
  
  //Exit Button 0
  SetUpBtns[0].BtnXpos = col2;//130;  //Button X position
  SetUpBtns[0].BtnWdth = btnWdthS;//80;  //Button Width
  SetUpBtns[0].BtnHght =btnHght;
  SetUpBtns[0].BtnYpos = row0;// scrnHeight - (SetUpBtns[0].BtnHght + 5);  //Button Y position
  SetUpBtns[0].Captn = "Exit";
  SetUpBtns[0].BtnClr = YELLOW; 
  SetUpBtns[0].TxtClr = WHITE;
//  //Exit Button position parameters
  SetUpBtns[1].BtnXpos = col3;//SetUpBtns[0].BtnXpos+ SetUpBtns[0].BtnWdth+1;  //Button X position
  SetUpBtns[1].BtnWdth = btnWdthS;//SetUpBtns[0].BtnWdth;  //Button Width
  SetUpBtns[1].BtnHght = btnHght;//SetUpBtns[0].BtnHght;
  SetUpBtns[1].BtnYpos = row0;//SetUpBtns[0].BtnYpos;  //Button Y position
  SetUpBtns[1].Captn = "Save";
  SetUpBtns[1].BtnClr = GREEN; 
  SetUpBtns[1].TxtClr = WHITE;

  //IncFreq Button parameters Button 2
  SetUpBtns[2].BtnXpos = col4;//SetUpBtns[1].BtnXpos+SetUpBtns[1].BtnWdth+1;  //Button X position
  SetUpBtns[2].BtnWdth = 80;  //Button Width
  SetUpBtns[2].BtnHght = 40;
  SetUpBtns[2].BtnYpos = row1;//scrnHeight - (SetUpBtns[0].BtnHght + SetUpBtns[2].BtnHght + 5);  //Button Y position
  SetUpBtns[2].Captn = "Freq+";
  SetUpBtns[2].BtnClr = BLUE; 
  SetUpBtns[2].TxtClr = WHITE;

  //DecFreq Button parameters Button 3
  SetUpBtns[3].BtnXpos = col1;//SetUpBtns[0].BtnXpos-(80+1);  //Button X position
  SetUpBtns[3].BtnWdth = 80;  //Button Width
  SetUpBtns[3].BtnHght = 40;
  SetUpBtns[3].BtnYpos = row1;//scrnHeight - (SetUpBtns[0].BtnHght+SetUpBtns[3].BtnHght + 5);  //Button Y position
  SetUpBtns[3].Captn = "Freq-";
  SetUpBtns[3].BtnClr = BLUE; 
  SetUpBtns[3].TxtClr = WHITE;

//IncLED Button parameters Button 4
  SetUpBtns[4].BtnXpos = col5;//SetUpBtns[2].BtnXpos+SetUpBtns[2].BtnWdth+1;  //Button X position
  SetUpBtns[4].BtnWdth = 80;  //Button Width
  SetUpBtns[4].BtnHght = 40;
  SetUpBtns[4].BtnYpos = row2;//SetUpBtns[2].BtnYpos-SetUpBtns[4].BtnHght;  //Button Y position
  SetUpBtns[4].Captn = "LED +";
  SetUpBtns[4].BtnClr = BLUE; 
  SetUpBtns[4].TxtClr = WHITE;

  //DecLED Button parameters  Button 5
  SetUpBtns[5].BtnXpos = col0;//SetUpBtns[0].BtnXpos-((2*80)+1);  //Button X position
  SetUpBtns[5].BtnWdth = 80;  //Button Width
  SetUpBtns[5].BtnHght = 40;
  SetUpBtns[5].BtnYpos = row2;//SetUpBtns[2].BtnYpos-SetUpBtns[5].BtnHght;  //Button Y position
  SetUpBtns[5].Captn = "LED -";
  SetUpBtns[5].BtnClr = BLUE; 
  SetUpBtns[5].TxtClr = WHITE;

  //IncTone Scale Factor Button Position parameters Button 6
  SetUpBtns[6].BtnXpos = col5;//SetUpBtns[4].BtnXpos;  //Button X position
  SetUpBtns[6].BtnWdth = 80;  //Button Width
  SetUpBtns[6].BtnHght = 40;
  SetUpBtns[6].BtnYpos = row3;//SetUpBtns[5].BtnYpos-SetUpBtns[6].BtnHght;  //Button Y position
  SetUpBtns[6].Captn = "TSF +";
  SetUpBtns[6].BtnClr = RED; 
  SetUpBtns[6].TxtClr = WHITE;

  

  //DecTone Scale Factor Button Position  parameters Button 7
  SetUpBtns[7].BtnXpos = col0;//SetUpBtns[5].BtnXpos;  //Button X position
  SetUpBtns[7].BtnWdth = 80;  //Button Width
  SetUpBtns[7].BtnHght = 40;
  SetUpBtns[7].BtnYpos = row3;//SetUpBtns[4].BtnYpos - SetUpBtns[7].BtnHght;  //Button Y position
  SetUpBtns[7].Captn = "TSF -";
  SetUpBtns[7].BtnClr = RED; 
  SetUpBtns[7].TxtClr = WHITE;


  //IncNoise Scale Factor Button Position parameters Button 8
  SetUpBtns[8].BtnWdth = 80;  //Button Width
  SetUpBtns[8].BtnHght = 40;
  SetUpBtns[8].BtnXpos = col4;//SetUpBtns[6].BtnXpos-(SetUpBtns[8].BtnWdth+1);  //Button X position
  SetUpBtns[8].BtnYpos = row3;//SetUpBtns[4].BtnYpos-SetUpBtns[8].BtnHght;  //Button Y position
  SetUpBtns[8].Captn = "NSF +";
  SetUpBtns[8].BtnClr = RED; 
  SetUpBtns[8].TxtClr = WHITE;
  
  //DecNoise Scale Factor Button Position  parameters Button 9
  SetUpBtns[9].BtnWdth = 80;  //Button Width
  SetUpBtns[9].BtnHght = 40;
  SetUpBtns[9].BtnXpos = col1;//SetUpBtns[7].BtnXpos+(SetUpBtns[7].BtnWdth+1);  //Button X position
  SetUpBtns[9].BtnYpos = row3;//SetUpBtns[4].BtnYpos-SetUpBtns[9].BtnHght;  //Button Y position
  SetUpBtns[9].Captn = "NSF -";
  SetUpBtns[9].BtnClr = RED; 
  SetUpBtns[9].TxtClr = WHITE;

  //IncBIAS Button parameters Button 10
  SetUpBtns[10].BtnXpos = col5;  //Button X position
  SetUpBtns[10].BtnWdth = btnWdthS;  //Button Width
  SetUpBtns[10].BtnHght = btnHght;
  SetUpBtns[10].BtnYpos = row4;  //Button Y position
  SetUpBtns[10].Captn = "BIAS+";
  SetUpBtns[10].BtnClr = YELLOW; 
  SetUpBtns[10].TxtClr = WHITE;

  //DecBIAS Button parameters Button 11
  SetUpBtns[11].BtnXpos = col0;  //Button X position
  SetUpBtns[11].BtnWdth = btnWdthS;  //Button Width
  SetUpBtns[11].BtnHght = btnHght;
  SetUpBtns[11].BtnYpos = row4;  //Button Y position
  SetUpBtns[11].Captn = "BIAS-";
  SetUpBtns[11].BtnClr = YELLOW; 
  SetUpBtns[11].TxtClr = WHITE;

  //IncSQLCH Button parameters Button 12
  SetUpBtns[12].BtnXpos = col4;  //Button X position
  SetUpBtns[12].BtnWdth = btnWdthS;  //Button Width
  SetUpBtns[12].BtnHght = btnHght;
  SetUpBtns[12].BtnYpos = row4;  //Button Y position
  SetUpBtns[12].Captn = "MSQL+";
  SetUpBtns[12].BtnClr = MAGENTA; 
  SetUpBtns[12].TxtClr = WHITE;  
    
  //DecSQLCH Button parameters Button 13
  SetUpBtns[13].BtnXpos = col1;  //Button X position
  SetUpBtns[13].BtnWdth = btnWdthS;  //Button Width
  SetUpBtns[13].BtnHght = btnHght;
  SetUpBtns[13].BtnYpos = row4;  //Button Y position
  SetUpBtns[13].Captn = "MSQL-";
  SetUpBtns[13].BtnClr = MAGENTA; 
  SetUpBtns[13].TxtClr = WHITE;  

  if (NoiseSqlch) sprintf(BtnCaptn1, "NOISE SQLCH");  
  else sprintf(BtnCaptn1, " MAN SQLCH");
  //SqMode Button parameters Button 14
  SetUpBtns[14].BtnXpos = col2;  //Button X position
  SetUpBtns[14].BtnWdth = btnWdthL;  //Button Width
  SetUpBtns[14].BtnHght = btnHght;
  SetUpBtns[14].BtnYpos = row4;  //Button Y position
  SetUpBtns[14].Captn = BtnCaptn1;
  SetUpBtns[14].BtnClr = MAGENTA; 
  SetUpBtns[14].TxtClr = WHITE;

  //Dfault Button parameters Button 15
  SetUpBtns[15].BtnXpos = col2;  //Button X position
  SetUpBtns[15].BtnWdth = btnWdthL;  //Button Width
  SetUpBtns[15].BtnHght = btnHght;
  SetUpBtns[15].BtnYpos = row3;  //Button Y position
  SetUpBtns[15].Captn = "FACTORY VALS";
  SetUpBtns[15].BtnClr = GREEN; 
  SetUpBtns[15].TxtClr = WHITE;
 
  
  //DeBug Button parameters Button 16
  SetDBgCptn(BtnCaptn);
  SetUpBtns[16].BtnXpos = col2;  //Button X position
  SetUpBtns[16].BtnWdth = btnWdthL;  //Button Width
  SetUpBtns[16].BtnHght = btnHght;
  SetUpBtns[16].BtnYpos = row2;  //Button Y position
  SetUpBtns[16].Captn = BtnCaptn;
  SetUpBtns[16].BtnClr = MAGENTA; 
  SetUpBtns[16].TxtClr = WHITE;

  //Freq Mode Button parameters Button 17
  if (AutoTune) sprintf(BtnCaptn2, " AUTO TUNE");  
  else sprintf(BtnCaptn2, "FREQ LOCKED");
  SetUpBtns[17].BtnXpos = col2;  //Button X position
  SetUpBtns[17].BtnWdth = btnWdthL;  //Button Width
  SetUpBtns[17].BtnHght = btnHght;
  SetUpBtns[17].BtnYpos = row1;  //Button Y position
  SetUpBtns[17].Captn = BtnCaptn2;
  SetUpBtns[17].BtnClr = RED; 
  SetUpBtns[17].TxtClr = WHITE;
  
  py = 0;
  px = 0;
  tchcnt = 100;
  enableDisplay();
  tft.fillScreen(BLACK);
  
  for(int i=0; i<18; i++) BldBtn(i, SetUpBtns); // Build the SetUp Button Set
  ShwUsrParams();
  delay(1000);
  setupflg = true;
  while(setupflg){ // run inside this loop until user exits setup mode
    //Serial.print("tchcnt: "); Serial.println(tchcnt);
    tchcnt -=1; //decrement "techcnt" by 1
    if(tchcnt==0){
      readResistiveTouch();
      tchcnt = 100;
    } else{
      //tp = {0, 0, 0};
      py = 0;
      px = 0;
    }
    //if(!ScrnPrssd & btnPrsdCnt > 0) btnPrsdCnt = 0;  
    if (tp.z > 200) { // if (tp.z > MINPRESSURE && tp.z < MAXPRESSURE) { //
      //use the following for Screen orientation set to 1
      py = map(tp.y, TS_BOT, TS_TOP, 0, scrnHeight);
      px = map(tp.x, TS_LEFT, TS_RT, 0, scrnWidth);

    }
    //if ((px > IncFrqX && px < IncFrqX+IncFrqWdth) && (py > IncFrqY && py < IncFrqY+IncFrqHght)&& buttonEnabled){
    if (BtnActive(2, SetUpBtns, px, py)){ // Inc Feq button was pressed
      LEDLpcnt -=1;
      if(LEDLpcnt ==0){
        LEDLpcnt = 20000;
        Scaning = false;
        //CalcFrqParams(TARGET_FREQUENCYC + 10.0);
        TARGET_FREQUENCYC += 10.0;
        if(TARGET_FREQUENCYC > 901.0) TARGET_FREQUENCYC -= 10.0; 
        CalcFrqParams(TARGET_FREQUENCYC);
        
      }
    }

    //if ((px > DecFrqX && px < DecFrqX+DecFrqWdth) && (py > DecFrqY && py < DecFrqY+DecFrqHght)&& buttonEnabled){
    if (BtnActive(3, SetUpBtns, px, py)){ // Dec Feq button was pressed
      LEDLpcnt -=1;
      if(LEDLpcnt ==0){
        LEDLpcnt = 20000;
        Scaning = false;
        TARGET_FREQUENCYC -= 10.0;
        if(TARGET_FREQUENCYC < 499.0) TARGET_FREQUENCYC += 10.0; 
        CalcFrqParams(TARGET_FREQUENCYC);
      }
    }

    //if ((px > IncLEDX && px < IncLEDX+IncLEDWdth) && (py > IncLEDY && py < IncLEDY+IncLEDHght)&& buttonEnabled){
    if (BtnActive(4, SetUpBtns, px, py)){
      // Increment LED button was pressed
      LEDLpcnt -=1;
      if(LEDLpcnt ==0){
        LEDLpcnt = 20000;
        DimFctr += 0.05;
        if(DimFctr>1.0) DimFctr= 1.0; 
        ShwUsrParams();
      }
      
    }

    //if ((px > DecLEDX && px < DecLEDX+DecLEDWdth) && (py > DecLEDY && py < DecLEDY+DecLEDHght)&& buttonEnabled){
    if (BtnActive(5, SetUpBtns, px, py)){
      // Decrement LED button was pressed
      LEDLpcnt -=1;
      if(LEDLpcnt ==0){
        LEDLpcnt = 20000;
        DimFctr -= 0.05;
        if(DimFctr<0.05) DimFctr= 0.05; 
        ShwUsrParams();
      }
    }

   //if ((px > IncTONEX && px < IncTONEX+IncTONEWdth) && (py > IncTONEY && py < IncTONEY+IncTONEHght)&& buttonEnabled){
   if (BtnActive(6, SetUpBtns, px, py)){
      // Increment TONE Scale Factor button was pressed
      LEDLpcnt -=1;
      if(LEDLpcnt ==0){
        LEDLpcnt = 20000;
        TSF += 0.05;
        if(TSF>3.0) TSF= 3.0; 
        ShwUsrParams();
      }
      
    }

    //if ((px > DecTONEX && px < DecTONEX+DecTONEWdth) && (py > DecTONEY && py < DecTONEY+DecTONEHght)&& buttonEnabled){
    if (BtnActive(7, SetUpBtns, px, py)){
      // Decrement TONE Scale Factor button was pressed
      LEDLpcnt -=1;
      if(LEDLpcnt ==0){
        LEDLpcnt = 20000;
        TSF -= 0.05;
        if(TSF<0.05) TSF= 0.05; 
        ShwUsrParams();
      }
    }

   //if ((px > IncNOISEX && px < IncNOISEX+IncNOISEWdth) && (py > IncNOISEY && py < IncNOISEY+IncNOISEHght)&& buttonEnabled){
   if (BtnActive(8, SetUpBtns, px, py)){
      // Increment NOISE Scale Factor button was pressed
      LEDLpcnt -=1;
      if(LEDLpcnt ==0){
        LEDLpcnt = 20000;
        NSF += 0.05;
        if(NSF>1.2) NSF= 1.2; 
        ShwUsrParams();
      }
      
    }

    //if ((px > DecNOISEX && px < DecNOISEX+DecNOISEWdth) && (py > DecNOISEY && py < DecNOISEY+DecNOISEHght)&& buttonEnabled){
    if (BtnActive(9, SetUpBtns, px, py)){
      // Decrement NOISE Scale Factor button was pressed
      LEDLpcnt -=1;
      if(LEDLpcnt ==0){
        LEDLpcnt = 20000;
        NSF -= 0.05;
        if(NSF<0.5) NSF= 0.5; 
        ShwUsrParams();
      }
    }    


    //if ((px > IncBIASX && px < IncBIASX+IncBIASWdth) && (py > IncBIASY && py < IncBIASY+IncBIASHght)&& buttonEnabled){
    if (BtnActive(10, SetUpBtns, px, py)){
      // Increment BIAS button was pressed
      //buttonEnabled = false;
      LEDLpcnt -=1;
      if(LEDLpcnt ==0){
        LEDLpcnt = 20000;
        BIAS += 10;
        if(BIAS>2500) BIAS= 2500; 
        ShwUsrParams();
      }
    }

    //if ((px > DecBIASX && px < DecBIASX+DecBIASWdth) && (py > DecBIASY && py < DecBIASY+DecBIASHght)&& buttonEnabled){
    if (BtnActive(11, SetUpBtns, px, py)){
      //buttonEnabled = false;
      // Decrement BIAS button was pressed
      LEDLpcnt -=1;
      if(LEDLpcnt ==0){
        LEDLpcnt = 20000;
        BIAS -= 10;
        if(BIAS<1500) BIAS= 1500; 
        ShwUsrParams();
      }
    }

    //if ((px > IncSQLCHX && px < IncSQLCHX+IncSQLCHWdth) && (py > IncSQLCHY && py < IncSQLCHY+IncSQLCHHght)&& buttonEnabled){
    if (BtnActive(12, SetUpBtns, px, py)){  
      // Increment SQLCH button was pressed
      //buttonEnabled = false;
      LEDLpcnt -=1;
      if(LEDLpcnt ==0){
        LEDLpcnt = 10000;
        ManSqlch += 100;
        if(ManSqlch>15000) ManSqlch= 15000; 
        ShwUsrParams();
      }
    }

    //if ((px > DecSQLCHX && px < DecSQLCHX+DecSQLCHWdth) && (py > DecSQLCHY && py < DecSQLCHY+DecSQLCHHght)&& buttonEnabled){
    if (BtnActive(13, SetUpBtns, px, py)){
      //buttonEnabled = false;
      // Decrement SQLCH button was pressed
      LEDLpcnt -=1;
      if(LEDLpcnt ==0){
        LEDLpcnt = 20000;
        ManSqlch -= 100;
        if(ManSqlch<500) ManSqlch= 500; 
        ShwUsrParams();
      }
    }

    //if ((px > SqModeX && px < SqModeX+SqModeWdth) && (py > SqModeY && py < SqModeY+SqModeHght)&& buttonEnabled){
    if (BtnActive(14, SetUpBtns, px, py) && buttonEnabled){
      // SQLCH Mode button was pressed
      buttonEnabled = false;
      NoiseSqlch = !NoiseSqlch;
      //DrwSQModeBtn(SqModeX, SqModeWdth, SqModeY, SqModeHght);
      if (NoiseSqlch) sprintf(BtnCaptn1, "NOISE SQLCH");  
      else sprintf(BtnCaptn1, " MAN SQLCH");
      SetUpBtns[14].Captn = BtnCaptn1;
      BldBtn(14, SetUpBtns);
      ShwUsrParams();
     
    }

    //if ((px > DeBugX && px < DeBugX+DeBugWdth) && (py > DeBugY && py < DeBugY+DeBugHght)&& buttonEnabled){
    if (BtnActive(16, SetUpBtns, px, py) && buttonEnabled){
      // Debug Mode button was pressed
      buttonEnabled = false;
      DeBug +=1;
      if(DeBug ==3) DeBug = 0;
      SetDBgCptn(BtnCaptn);
      SetUpBtns[16].Captn = BtnCaptn;
      BldBtn(16, SetUpBtns);
      //DrwDBModeBtn(DeBugX, DeBugWdth, DeBugY, DeBugHght);
      ShwUsrParams();
      
    }

    //if ((px > DfaultX && px < DfaultX+DfaultWdth) && (py > DfaultY && py < DfaultY+DfaultHght)&& buttonEnabled){
    if (BtnActive(15, SetUpBtns, px, py) && buttonEnabled){
      // Restore Defaults button was pressed
      buttonEnabled = false;
      SetUpBtns[15].BtnClr = BLACK;
      BldBtn(15, SetUpBtns);
      //DrawBtn(DfaultX, DfaultWdth, DfaultY, DfaultHght, "FACTORY VALS", BLACK, WHITE);
      DimFctr = DFault.DimFctr;
      TSF = DFault.TSF;
      NSF = DFault.NSF; 
      BIAS = DFault.BIAS;
      DeBug = DFault.DeBug; 
      ManSqlch = DFault.ManSqlch; 
      NoiseSqlch = DFault.NoiseSqlch;
      AutoTune = DFault.AutoTune;
      TARGET_FREQUENCYC = DFault.TARGET_FREQUENCYC;
      //DrwSQModeBtn(SqModeX, SqModeWdth, SqModeY, SqModeHght);
      if (NoiseSqlch) sprintf(BtnCaptn1, "NOISE SQLCH");  
      else sprintf(BtnCaptn1, " MAN SQLCH");
      SetUpBtns[14].Captn = BtnCaptn1;
      BldBtn(14, SetUpBtns);
      //DrwDBModeBtn(DeBugX, DeBugWdth, DeBugY, DeBugHght);
      SetDBgCptn(BtnCaptn);
      SetUpBtns[16].Captn = BtnCaptn;
      BldBtn(16, SetUpBtns);
      if (AutoTune) sprintf(BtnCaptn2, " AUTO TUNE");  
      else sprintf(BtnCaptn2, "FREQ LOCKED");
      SetUpBtns[17].Captn = BtnCaptn2;
      BldBtn(17, SetUpBtns);
      ShwUsrParams();
      delay(150);
      SetUpBtns[15].BtnClr = GREEN;
      BldBtn(15, SetUpBtns);
      //DrawBtn(DfaultX, DfaultWdth, DfaultY, DfaultHght, "FACTORY VALS", GREEN, WHITE);
      //}
    }

    if (BtnActive(17, SetUpBtns, px, py) && buttonEnabled){
      // SQLCH Mode button was pressed
      buttonEnabled = false;
      AutoTune = !AutoTune;
      Serial.println(AutoTune);
      //DrwSQModeBtn(SqModeX, SqModeWdth, SqModeY, SqModeHght);
      if (AutoTune) sprintf(BtnCaptn2, " AUTO TUNE");  
      else sprintf(BtnCaptn2, "FREQ LOCKED");
      SetUpBtns[17].Captn = BtnCaptn2;
      BldBtn(17, SetUpBtns);
      py = 0;
      px = 0;
      //ShwUsrParams();
     
    }
    
    //if ((px > ExitX && px < ExitX+ExitWdth) && (py > ExitY && py < ExitY+ExitHght)&& buttonEnabled){
    if (BtnActive(0, SetUpBtns, px, py) && buttonEnabled){  
      buttonEnabled = false;
      setupflg = false;// exit button was pressed
    }
    //if ((px > SaveX && px < SaveX+SaveWdth) && (py > SaveY && py < SaveY+SaveHght)&& buttonEnabled){
    if (BtnActive(1, SetUpBtns, px, py)&& buttonEnabled){  
      buttonEnabled = false;
      saveflg = true;
      // Save button was pressed. So store current User params to virtual EEPROM (STM flash memory)
      //Draw SaveBtn
      // make "SAVE" button "BLACK" to give a visual confirmation of button press
      SetUpBtns[1].BtnClr = BLACK;
      BldBtn(1, SetUpBtns);  
      float NewToneFreq = TARGET_FREQUENCYC;
      int n= EEPROM_write(EEaddress+0, NewToneFreq);
      StrdDimFctr = DimFctr;
      n = EEPROM_write(EEaddress+4, StrdDimFctr);
      n = EEPROM_write(EEaddress+8, TSF);
      n = EEPROM_write(EEaddress+12, BIAS);
      n = EEPROM_write(EEaddress+16, ManSqlch);
      n = EEPROM_write(EEaddress+20, NoiseSqlch);
      n = EEPROM_write(EEaddress+24, DeBug);
      n = EEPROM_write(EEaddress+28, NSF);
      n = EEPROM_write(EEaddress+32, AutoTune);
      //Serial.print("N: ");Serial.print(n);
  
      delay(250);
      //ReDraw SaveBtn
      SetUpBtns[1].BtnClr = GREEN;
      BldBtn(1, SetUpBtns);
    }
    loopcnt -=1;
    if (loopcnt == 0){
      loopcnt = 80000;
      cursorX = 0;
      cursorY = 2 * (fontH + 10);
      char StatMsg[40];
      sprintf( StatMsg, "Tone Mag: %d", (long)mag );
      ShwData(cursorX, cursorY, StatMsg);
      sprintf( StatMsg, "Noise: %d", (long)noise);
      ShwData(16 *fontW, cursorY, StatMsg);
      

    }// end if(loopcnt == 0)
    if( ToneLvl > SqlchVal) NoSigFlg = true;
    if(NoSigFlg && ( ToneLvl < SqlchVal)) {
      noSigStrt =  millis();
      NoSigFlg = false;
    }
  }//End While Loop
//  int n= EEPROM_read(EEaddress+0, StrdFREQUENCY);
//  if(StrdFREQUENCY !=0 && n == 4){
//    TARGET_FREQUENCYC = StrdFREQUENCY; //Hz
//    TARGET_FREQUENCYL = feqlratio*StrdFREQUENCY; //Hz
//    TARGET_FREQUENCYH = feqhratio*StrdFREQUENCY; //Hz
//    InitGoertzel();
//    coeff = coeffC;
//    N = NC;
//  }
//  StrdDimFctr = 0;
//  n = EEPROM_read(EEaddress+4, StrdDimFctr);
//  if(StrdDimFctr > 0 && n == 4){
//    DimFctr = StrdDimFctr;
//  }
  SftReset();
  SetRtnflg = true;
  SCD = CurSCD; //restore SCD flag 
  return;
}//end of SetUp Loop

//////////////////////////////////////////////////////////////////////////

void ShwData(int MsgX, int MsgY, char* TxtMsg){
      //Serial.println(StatMsg);
      int msgpntr = 0;
      //int cnt = 0;
      //int offset = 0;
      tft.setCursor(MsgX, MsgY);
      while (TxtMsg[msgpntr] != 0) {
        tft.fillRect(MsgX, MsgY, fontW+4, (fontH + 10), BLACK);
        char curChar =TxtMsg[msgpntr];
        tft.print(curChar);
        msgpntr++;
        //cnt++;
        MsgX += (fontW);
//        if (((cnt) - offset)*fontW >= displayW) {
//          curRow++;
//          MsgX = 0;
//          MsgY = curRow * (fontH + 10);
//          offset = cnt;
//          tft.setCursor(MsgX, MsgY);
//        }
//        else MsgX = (cnt - offset) * fontW;
        tft.setCursor(MsgX, MsgY);
      }
      while (msgpntr<39) {
        tft.fillRect(MsgX, MsgY, fontW+4, (fontH + 10), BLACK);
        msgpntr++;
        //cnt++;
        //MsgX = (cnt - offset) * fontW;
        MsgX += (fontW);
      }
}

//////////////////////////////////////////////////////////////////////////
void CalcFrqParams(float NewToneFreq){
  //float NewToneFreq = TARGET_FREQUENCYC + 10.0;
  TARGET_FREQUENCYC = NewToneFreq; //Hz
  TARGET_FREQUENCYL = feqlratio*NewToneFreq; //Hz
  TARGET_FREQUENCYH = feqhratio*NewToneFreq; //Hz
  InitGoertzel();
  coeff = coeffC;
  N = NC;
  // now Display current freq
  if(setupflg && !Scaning)ShwUsrParams();
}
//////////////////////////////////////////////////////////////////////////

void ShwUsrParams()
{
  int ToneCfreq = int(TARGET_FREQUENCYC);
  int ToneDeci = int(10*(TARGET_FREQUENCYC-ToneCfreq));
  int DimFctrInt = int(DimFctr);
  int DimFctrDeci = int(100*(DimFctr-DimFctrInt));
  char StatMsg[40];
  //Serial.print("TARGET_FREQUENCYC: "); Serial.println(TARGET_FREQUENCYC);
  //sprintf( Title, "Current Center Freq: %d.%d Hz", ToneCfreq, ToneDeci );
  sprintf( StatMsg, "Center Freq: %d.%d Hz; LEDBright:%d.%d", ToneCfreq, ToneDeci, DimFctrInt, DimFctrDeci );
  tft.setCursor(textSrtX, textSrtY);
  for ( int i = 0; i <  sizeof(Pgbuf);  ++i )
    Pgbuf[i] = 0;
  cnt = 0;
  curRow = 0;
  offset = 0;
  cursorX = 0;
  cursorY = 0;
  tft.fillRect(cursorX, cursorY, scrnWidth, (1 * (fontH + 10)), BLACK); //erase top 2 rows
  dispMsg(StatMsg);
  
  cursorX = 0;
  cursorY = 1 * (fontH + 10);
  int TSFintval = (int)TSF;
  int TSFfract = 100*(TSF-TSFintval);
  sprintf( StatMsg, "TSF:%d.%d; ", TSFintval, TSFfract);
  ShwData(cursorX, cursorY, StatMsg);

  cursorX = 10*fontW;
  //cursorY = 1 * (fontH + 10);
  sprintf( StatMsg, "BIAS:%d; ", BIAS);
  ShwData(cursorX, cursorY, StatMsg);
  cursorX = 21*fontW;
  //cursorY = 1 * (fontH + 10);
  sprintf( StatMsg, "MSQL:%d; ", ManSqlch);
  ShwData(cursorX, cursorY, StatMsg);

  cursorX = 32*fontW;
  //cursorY = 1 * (fontH + 10);
  int NSFintval = (int)NSF;
  int NSFfract = 100*(NSF-NSFintval);
  sprintf( StatMsg, "NSF:%d.%d; ", NSFintval, NSFfract);
  ShwData(cursorX, cursorY, StatMsg);
  return;
}
//////////////////////////////////////////////////////////////////////////
void DrawBtn(int Bposx, int Bwidth, int Bposy, int Bheight, const char* Captn, unsigned int BtnClr, unsigned int TxtClr){
  //if (scrnHeight == 320) Bposy = scrnHeight - (Bheight + 5);
  //if (scrnWidth == 480)  Bposx = Bposx + 32; //Bposx = Bposx +20;
  tft.fillRect(Bposx, Bposy, Bwidth, Bheight, BtnClr);
  tft.drawRect(Bposx, Bposy, Bwidth, Bheight, WHITE);
  tft.setCursor(Bposx + 11, Bposy + 12);
  tft.setTextColor(TxtClr);
  tft.setTextSize(2);
  tft.print(Captn);

}

//////////////////////////////////////////////////////////////////////////
void BldBtn(int BtnNo, BtnParams Btns[]){ //, const char* Captn, unsigned int BtnClr, unsigned int TxtClr){
  tft.fillRect(Btns[BtnNo].BtnXpos, Btns[BtnNo].BtnYpos, Btns[BtnNo].BtnWdth, Btns[BtnNo].BtnHght, Btns[BtnNo].BtnClr);
  tft.drawRect(Btns[BtnNo].BtnXpos, Btns[BtnNo].BtnYpos, Btns[BtnNo].BtnWdth, Btns[BtnNo].BtnHght, WHITE);
  tft.setCursor(Btns[BtnNo].BtnXpos + 11, Btns[BtnNo].BtnYpos + 12);
  tft.setTextColor(Btns[BtnNo].TxtClr);
  tft.setTextSize(2);
  tft.print(Btns[BtnNo].Captn);

}
///////////////////////////////////////////////////////////////////////////
bool BtnActive(int BtnNo, BtnParams Btns[], int px, int py){
  bool actv = false;
  if ((px > Btns[BtnNo].BtnXpos && px < Btns[BtnNo].BtnXpos+Btns[BtnNo].BtnWdth) && (py > Btns[BtnNo].BtnYpos && py < Btns[BtnNo].BtnYpos+Btns[BtnNo].BtnHght)) actv = true;
  return actv;
}
///////////////////////////////////////////////////////////////////////////

void  SftReset(){
  buttonEnabled = false; //Disable button
  ClrBtnCnt = 0;
  enableDisplay();
  tft.fillScreen(BLACK);
  DrawButton();
  Button2();
  WPMdefault();
  px = 0;
  py = 0;
  showSpeed();
  tft.setCursor(textSrtX, textSrtY);
  for ( int i = 0; i <  sizeof(Pgbuf);  ++i )
    Pgbuf[i] = 0;
  cnt = 0;
  curRow = 0;
  offset = 0;
  cursorX = 0;
  cursorY = 0;
  return;
}
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
//    Serial.print(DeCodeVal);
    if (NuWrd) lastWrdVal = millis() - WrdStrt;
    if ((deadSpace > 15) && (deadSpace < 240) && (!NuWrd)) { // looks like we have a legit dead space interval(its between 5 & 50 WPM)
      if (Bug2) {
        if (deadSpace < avgDit && deadSpace > avgDit / 4) {
          avgDeadSpace = (15 * avgDeadSpace + deadSpace) / 16;
        }
      } else {
        if ((deadSpace < lastDah) && (DeCodeVal != 1)) { //20200817 added "DeCodeVal != 1" for more consistent letter break calcs
          //if (DeCodeVal != 1) { //ignore letterbrk dead space intervals
          if(ltrCmplt < -350){  //ignore letterbrk dead space intervals
            //avgDeadSpace = (3 * avgDeadSpace + deadSpace) / 4;
            avgDeadSpace = (5 * avgDeadSpace + deadSpace) / 6;
            //Serial.print("avgDeadSpace1: ");
            //Serial.print(deadSpace);
            //Serial.print('\t');
          } else {
            AvgLtrBrk = ((9 * AvgLtrBrk) + deadSpace) / 10;
            //Serial.print("AvgLtrBrk");
            //Serial.print('\t');
          }
        }
        if (NrmMode && (avgDeadSpace < avgDit)&& !Bug3) { // running Normall mode; use Dit timing to establish minmum "space" interval
          if(ltrCmplt < -350){  //ignore letterbrk dead space intervals
            avgDeadSpace = avgDit; //ignore letterbrk dead space intervals
            //Serial.print("avgDeadSpace2: ");
            //Serial.print(avgDit);
            //Serial.print('\t');
          }
//          Serial.print("  Path 3: ");
//          Serial.print(avgDeadSpace);
        }
      }
    }
    //Serial.println("");
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
  if(Bug3){
    if(LtrCntr < 9) LtrCntr++;
    for( int i = 9; i>0; i--){
      ShrtBrk[i] =ShrtBrk[i-1];
    }
  }
  //Figure out how much time to allow for a letter break
  if (Bug2) {
    space = ((3 * space) + avgDeadSpace) / 4;
  }
  else {
    if (avgDeadSpace > avgDit) space = avgDeadSpace; //((3*space)+avgDeadSpace)/4; //20190717 jmh - Changed to averaging space value to reduce chance of glitches causing mid character letter breaks
    else space =  ((3 * space) + avgDit) / 4; //20190717 jmh - Changed to averaging space value to reduce chance of glitches causing mid character letter breaks
  }
  
  if(wpm<35){
    //ltrBrk =  int(1.6*(float(space))); //int(1.7*(float(space))); //int(1.5*(float(space))); // Basic letter break interval //20200220 changed to 1.6 because was getting "L" when it should hve read "ED"
    ltrBrk =  int(1.5*(float(space)));// 20200306 went from 1.6 back to 1.5 to reduce the chance of having to deal with multi letter symbol groups 
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
  
  //if (BugMode) letterBrk = letterBrk + 0.8 * avgDit ;//20200306 removed to reduce having to deal with multi letter sysmbols
  if (NuWrd && NuWrdflg) {
    NuWrd = false;
    NuWrdflg = false;
  }
}
////////////////////////////////////////////////////////////////////////
void chkChrCmplt() {
  state = 0;
  unsigned long Now = millis();
  if((Now - letterBrk1) > 35000) letterBrk1 = Now - 10000;// keep "letterBrk1" from becoming an absurdly large value  
  //unsigned long Now = micros();
  //check to see if enough time has passed since the last key closure to signify that the character is complete
  if ((Now >= letterBrk) && letterBrk != 0 && DeCodeVal > 1) {
    state = 1; //have a complete letter
    ltrCmplt = -350;
    letterBrk1 = letterBrk;

    //Serial.println("   ");//testing only; comment out when running normally
    //Serial.println(Now-letterBrk);
    //Serial.println(letterBrk);
  }
  float noKeySig = (float)(Now - noSigStrt);
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
  
  //if(magC<10000) return thisdur; //JMH 02020811 Don't do anything with this period. Signal is too noisy to evaluate
  if( SNR < 4.0)  return thisdur; //JMH 02021004 Don't do anything with this period. Signal is too noisy to evaluate
  //if (thisdur > 3.4 * avgDit) thisdur = 3.4 * avgDit; //limit the effect that a single sustained "keydown" event can have 
//  Serial.print(thisdur);
  int fix = 0;
  bool UpDtDeadSpace = true;
  BadDedSpce = 0;
  //Serial.print('\t');
  if(DeCodeVal < 4){ // we're testing the first symbol in a letter and the current dead space is likely a word break or letter break 
    if(deadSpace > 700){// huge gap & occurred between this symbol and last. Could be a new sender. Reset and start over
      BadDedSpceCnt = 0;
      //Serial.print(deadSpace);
      //Serial.println("  Start Over");
      return thisdur;
    }
    UpDtDeadSpace = false;
    BadDedSpce = deadSpace;
    deadSpace = avgDeadSpace;
    if(curRatio < 2.5 && (float(3600/avgDah)< 14)){// We're running at a slow WPM but the ditto dah ratio is looking "whaky". Let's speed things up 
       BadDedSpceCnt = 0;
       avgDah = thisdur;
       avgDit = avgDah/3;
       //Serial.print("SpdUp");
    }
    else{
    
      BadSpceStk[BadDedSpceCnt] = BadDedSpce;
      BadDedSpceCnt +=1;
      //AvgBadDedSpce = (2*AvgBadDedSpce+BadDedSpce)/3;
      if(BadDedSpceCnt == 5){// we've been down this path 3 times in a row, so something is wrong; time to recalibrate
         BadDedSpceCnt = 0;
         UpDtDeadSpace = true;
          //deadSpace = AvgBadDedSpce;
         // out of the last three space intervals find the shortest two and average their intervals and use that as the current "deadSpace" value 
         if((BadSpceStk[2]>BadSpceStk[3])&& (BadSpceStk[2]>BadSpceStk[4])) deadSpace = (BadSpceStk[3]+BadSpceStk[4])/2;
         else if((BadSpceStk[3]>BadSpceStk[2])&& (BadSpceStk[3]>BadSpceStk[4])) deadSpace = (BadSpceStk[2]+BadSpceStk[4])/2;
         else if((BadSpceStk[4]>BadSpceStk[2])&& (BadSpceStk[4]>BadSpceStk[3])) deadSpace = (BadSpceStk[3]+BadSpceStk[3])/2;
         LastdeadSpace = deadSpace;
         if (thisdur < 2*deadSpace){// This "key down" interval looks like a dit
          if(3*thisdur > 1.5*avgDah){//Well, we thought was a dit, if we treat it as such, it will cause a seismic shift downward. So let's proceed with caution
            avgDah = 1.5*avgDah;
            avgDit = avgDah/3;
            //Serial.print("???");
          }
          else{
          avgDah = 3*thisdur;
          avgDit = thisdur;
          //Serial.print("NDT");// "Not a Dit"
          }
         }
         else{// This "key down" interval looks like a Dah
          avgDah = thisdur;
          avgDit = thisdur/3;
          
         }
         //Serial.print('#');
         //Serial.print(BadSpceStk[4]);
      } else{
        
        //Serial.print(BadDedSpce);
        //Serial.print('\t');
        //Serial.print(avgDeadSpace);
//        Serial.print("\tNuWrd = ");
//        if(NuWrd)Serial.println("true");
//        else Serial.println("false");
//        Serial.print("\t1st");
      }
    }
  }
  else{ //DecodeVal >= 4; we're analyzing Symbol timing of something other than 'T' or 'E' 
    if(thisdur > 0.7 * deadSpace){
      //Serial.print("Mid");// this is pretty commom path
      BadDedSpceCnt = 0; 
    }
    else{
      if (((deadSpace > 2.5*LastdeadSpace)||(deadSpace < avgDeadSpace/4)) && (LastdeadSpace != 0) ){
        UpDtDeadSpace = false;
        BadDedSpce = deadSpace;
        deadSpace = avgDeadSpace;
        BadSpceStk[BadDedSpceCnt] = BadDedSpce;
        BadDedSpceCnt +=1;
        //AvgBadDedSpce = (2*AvgBadDedSpce+BadDedSpce)/3;
        if(BadDedSpceCnt == 3){// we've been down this path 3 times in a row, so something is wrong; time to recalibrate (never see this side go true)
           BadDedSpceCnt = 0;
           UpDtDeadSpace = true;
           //deadSpace = AvgBadDedSpce;
           if((BadSpceStk[0]>BadSpceStk[1]) && (BadSpceStk[0]>BadSpceStk[2])) deadSpace = (BadSpceStk[1]+BadSpceStk[2])/2;
           else if((BadSpceStk[1]>BadSpceStk[0]) && (BadSpceStk[1]>BadSpceStk[2])) deadSpace = (BadSpceStk[0]+BadSpceStk[2])/2;
           else if((BadSpceStk[2]>BadSpceStk[0]) && (BadSpceStk[2]>BadSpceStk[1])) deadSpace = (BadSpceStk[0]+BadSpceStk[1])/2;
           //Serial.print('%');
        }
      }else{
        //Serial.print('!');// this occasionally happens
        BadDedSpceCnt = 0;
      }
    }
  

  }
  //use current deadSpace value to see if thisdur is a dit
  if(thisdur < 1.5 * deadSpace && thisdur > 0.5 * deadSpace ){//Houston, we have a "DIT"
    avgDit = (5 * avgDit + thisdur) / 6; //avgDit = (3 * avgDit + thisdur) / 4;
    fix += 1;
  }
  else if(thisdur < 1.5 *3* deadSpace && thisdur > 0.5 *3* deadSpace ){ //lets try to use the current deadSpace value to see if thisdur is a dah
    //it sure smells like a DAH
    avgDah = (5 * avgDah + thisdur) / 6; //avgDah = (3 * avgDah + thisdur) / 4;
    fix += 2; 
  }
  else // doesn't fit either of the above cases, so lets try something else; the following tests rarely get used
  {
    if (thisdur > 2 * avgDah) thisdur = 2 * avgDah; //first, set a max limit to aviod the absurd
    if (thisdur > avgDah){
      avgDah = ((2*avgDah)+thisdur)/3;
      fix += 3;
    }
    else if (thisdur < avgDit){
      avgDit = ((2*avgDit)+thisdur)/3;
      fix += 4; 
    }
    else {// this duration is somewhere between a dit & a dah
      if (thisdur > avgDah/2){
        avgDah = ((12*avgDah)+thisdur)/13;
        fix += 5;
      }
      else{
        avgDit = ((9*avgDit)+thisdur)/10;
        fix += 6;
      }
    }
  }
  if(UpDtDeadSpace) LastdeadSpace = deadSpace;
// old code
//  if (thisdur > 1.5 * avgDah) thisdur = 1.5 * avgDah; //jmh 20200923 raised the limit to speed up the time needed to adjust to slow code
//  if (thisdur < 0.5 *avgDah){ // this appears to be a "dit"
//    if(thisdur > 1.15 *avgDit )  avgDit = (3 * avgDit + thisdur) / 4; //jmh 20201012 raised the limit to speed up the time needed to adjust to slow code
//    else if(thisdur < avgDah/4 )avgDit = (2 * avgDit + thisdur) / 3;  //jmh 20201012 this line to reduce the time needed to correct for faster code
//    else avgDit = (4 * avgDit + thisdur) / 5;//(9 * avgDit + thisdur) / 10;
//  }
//  else if(thisdur> 1.12*avgDah) avgDah = ((2*avgDah)+thisdur)/3; //jmh 20201005 added this line to further to reduce the time needed to correct for slow code
//  else avgDah = ((4*avgDah)+thisdur)/5; //jmh 20200923 added this line to further to reduce the time needed to correct for slow code 
//end old code

  //Serial.print('\t');
  //Serial.print(thisdur);
  //Serial.print('\t');
  //Serial.print(deadSpace);
  if(!UpDtDeadSpace){
    //Serial.print('*');
    //Serial.print(BadDedSpce);
  }
  //Serial.print('\t');
  //Serial.print(avgDeadSpace);
  //Serial.print('\t');
  //Serial.print(avgDit);
  //Serial.print('\t');
  //Serial.print(avgDah);
  //Serial.print('\t');
  //Serial.print(float(3600/avgDah));
  curRatio = (float)avgDah / (float)avgDit;
  if(curRatio <2.5 && Bug3){
    curRatio = 2.5; 
    avgDit = avgDah/curRatio;
     //Serial.print('*');
  }
  //Serial.print('\t');
  //Serial.print(curRatio);
  //Serial.print('\t');
  //Serial.print(DeCodeVal);
  //Serial.print('\t');
  //Serial.println(fix);
    //set limits on the 'avgDit' values; 80 to 3.1 WPM
  if (avgDit > 384) avgDit = 384;
  if (avgDit < 15) avgDit = 15;
  if (DeCodeVal == 1) DeCodeVal = 0;
  //      if(Test){
  //        Serial.print(DeCodeVal);
  //        Serial.print(";  ");
  //        Serial.println("Valid");
  //      }
  //thisdur = avgDah / curRatio;
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
      Bug3 = false;
      break;
    case 1:
      BugMode = false;
      Bug2 = false;
      NrmMode = true;
      if(!Bug3){
        ShrtBrkA = ltrBrk;
        letterBrk1 = millis()-100;//initialize "letterBrk1" so the first "KeyEvntSR()" interupt doesn't generate an absurd value
        LtrCntr = 0;
        ShrtFctr = 0.48;
        AvgShrtBrk = ShrtBrkA;//initialize  AvgShrtBrk with a resonable value
      }
      Bug3 = true;
     
      break;  
    case 2:
      BugMode = true;
      NrmMode = false;
      Bug3 = false;
      break;
    case 3:
      BugMode = false;
      Bug2 = true;
      NrmMode = false;
      Bug3 = false;
      break;
  }
}
//////////////////////////////////////////////////////////////////////

void DisplayChar(unsigned int decodeval) {
  char curChr = 0 ;
  int pos1 = -1;
//  // slide all values in the CodeValBuf to the left by one position & make sure that the array is terminated with a zero in the last position
//  for (int i = 1; i < 7; i++) {
//    CodeValBuf[i - 1] = CodeValBuf[i];
//  }
//  CodeValBuf[6] = 0;

  if (decodeval == 2 || decodeval == 3) ++TEcnt;
  else TEcnt = 0;
  if (Test && !Bug3){
    Serial.print(decodeval);
    Serial.print("\t");
  }
  //clear buffer
  for ( int i = 0; i < sizeof(Msgbuf);  ++i )
    Msgbuf[i] = 0;
  //if((decodeval==255) & dletechar) dletechar = false; // this should be ok to do here, because if decodeval = 255 we will be passing one & only one character over to the "dispMsg()" routine 
  
  if(decodeval != 99999){
    DCVStrd[0] = decodeval;
    pos1 = linearSearchBreak(decodeval, CodeVal1, ARSIZE); // note: decodeval '255' returns SPACE character
    if(pos1<0){// did not find a match in the standard Morse table. So go check the extended dictionary
      pos1 = linearSearchBreak(decodeval, CodeVal2, ARSIZE2);
  //    Serial.print(pos1+1);
  //    Serial.print("\t");
      if(pos1<0){
        //sprintf( Msgbuf, "decodeval: %d ", decodeval);//use when debugging Dictionary table(s)
        sprintf( Msgbuf, "*");
//        if (Bug3){
//          float ShrtFctrold = ShrtFctr;
//          ShrtFctr = float(float(ltrBrk)/float(UsrLtrBrk));
//          if(ShrtFctr > ShrtFctrold) ShrtFctr = ShrtFctrold -0.05; // one way or the other we need to reduce "ShrtFctr"
//          ShrtBrkA =  ShrtFctr*UsrLtrBrk;
////          AvgShrtBrk = ShrtBrkA;
//
////          if( ShrtBrkA < UsrLtrBrk) {// recalibrate the timing settings 
////            ShrtFctr = float(float(ShrtBrkA)/float(UsrLtrBrk));//ShrtFctr = float(float(ShrtBrk[LtrCntr])/float(UsrLtrBrk));
////            AvgShrtBrk = ShrtBrk[LtrCntr];
////            ShrtBrkA =  ShrtFctr*UsrLtrBrk;
////          }else ShrtFctr -= 0.05;
//        }
        if (Test && Bug3){
          Serial.print("decodeval: ");
          Serial.println(decodeval);
        }
      }
      else sprintf( Msgbuf, "%s", DicTbl2[pos1] );
    }
    else sprintf( Msgbuf, "%s", DicTbl1[pos1] );
    MsgChrCnt[0] = 0;
    while(Msgbuf[MsgChrCnt[0]] != 0) ++MsgChrCnt[0]; // record how many characters will be printed in this group 
    /*(normally 1, but the exented dictionary can have many) 
     * Will use this later if deletes are needed
     */
    ConcatSymbl = false;
    if (Msgbuf[0] == 'E' || Msgbuf[0] == 'T') ++badCodeCnt;
    else if(decodeval !=255) badCodeCnt = 0;
    if (badCodeCnt > 5 && wpm > 25){ // do an auto reset back to 15wpm
      WPMdefault();
    }
  }else{
    sprintf( Msgbuf, "");
    dletechar = true;
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
  dispMsg(Msgbuf); // print current character(s) to LCD display
  int avgntrvl = int(float(avgDit + avgDah) / 4);
  //wpm = CalcWPM(avgntrvl);//use all current time intervalls to extract a composite WPM
  wpm = CalcWPM(avgDit, avgDah, avgDeadSpace);
  if (wpm != 1) { //if(wpm != lastWPM & decodeval == 0 & wpm <36){//wpm != lastWPM
    if (curChr != ' ') showSpeed();
    if (TEcnt > 7 &&  curRatio > 4.5) { // if true, we probably not waiting long enough to start the decode process, so reset the dot dash ratio based o current dash times
      avgDit = avgDah / 3;
    }
  }
  // slide all values in the CodeValBuf to the left by one position & make sure that the array is terminated with a zero in the last position
  for (int i = 1; i < 7; i++) {
    CodeValBuf[i - 1] = CodeValBuf[i];
  }
  CodeValBuf[6] = 0;
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
int linearSrchChr(char val, char arr[ARSIZE][2], int sz)
{
  int pos = -1;
  //Serial.print(val);
  //Serial.print(";\t");
  for (int i = 0; i < sz; i++)
  {
    char tstchar = arr[i][0];
    //Serial.print(tstchar);
    
    if(tstchar==val){
      pos = i;
      break;
    }
  }
  return pos;
}

//////////////////////////////////////////////////////////////////////
void dispMsg(char Msgbuf[50]) {
  //Serial.println(Msgbuf);
  if (Test && !Bug3) Serial.println(Msgbuf);
  int msgpntr = 0;
  int xoffset = 0;
  char info[25];
  // sprintf(info, "");
  info[0] = 0;
  enableDisplay();
  while ( Msgbuf[msgpntr] != 0) {
    if((Msgbuf[msgpntr]==' ') & dletechar){
      dletechar = false;
      if(Bug3  & SCD && Test) Serial.println("Kill Delete");
    }
    if(dletechar){ // we need to erase the last displayed character
      dletechar = false;
      if(Bug3 && SCD && Test) sprintf( info, "*Replace Last Char*"); //Serial.print("Replace Last Char :");
      while(MsgChrCnt[1] !=0){// delete display of ever how many characters were printed in the last decodeval (may be more than one letter generated)  
        //first,buzz thru the pgbuf array until we find the the last charater (delete the last character in the pgbuf)
        int TmpPntr = 0;
        while(Pgbuf[TmpPntr]!=0) TmpPntr++;
        if(TmpPntr>0) Pgbuf[TmpPntr-1] =0;// delete last character in the array by replacing it with a "0"
        //TmpPntr =0;
        cnt--;
        xoffset = cnt;
        //use the xoffset to locate the character position (on the display's x axis)
        //int DelRow = 0;
        curRow = 0;
        while (xoffset >= CPL){
          xoffset -=CPL;
          //DelRow++;
          curRow++; 
        }
        //int Xpos =  xoffset*(fontW);
        cursorX  =  xoffset*(fontW);
        //cursorY = DelRow * (fontH + 10);
        cursorY = curRow * (fontH + 10);
        if(xoffset==(CPL-1)) offset= offset-CPL; //we just jump back to last letter in the previous line, So we need setup to properly calculate what display row we will be on, come the next character
        tft.fillRect(cursorX, cursorY, fontW+4, (fontH + 10), BLACK); //black out/erase last displayed character
        tft.setCursor(cursorX, cursorY);
        --MsgChrCnt[1];
      }
    
    }//end delete character
    else{
//      if(newRow & SCD)Serial.println(" newRow Flag Cleared");
//      newRow = false;
      tft.setCursor(cursorX, cursorY);
    }
    

    MsgChrCnt[1] =MsgChrCnt[0];
    DCVStrd[1] = DCVStrd[0];// used by the KeyEvntSR()routine to facilitate grabbing back the last key sequence data received
    char curChar = Msgbuf[msgpntr];
    tft.print(curChar);
    //now test/correct letter groups that represent common mis-prints  
    if (curRow > 0) sprintf ( Pgbuf, "%s%c", Pgbuf, curChar);  // add the character just "printed" to the "PgBuf" 
    
    if (cnt>CPL){//Pgbuf now has enough data, to test for special character combos often found with sloppy sending
      if(Pgbuf[cnt-(CPL+1)]== 'P'  && Pgbuf[cnt-(CPL)]=='D'){ //test for "PD"
        sprintf ( Msgbuf, " (%c%s", Pgbuf[cnt-(CPL+2)], "AND)"); //"true"; Insert preceeding character plus correction "AND"
      }
      if(Pgbuf[cnt-(CPL+1)]== '6'  && Pgbuf[cnt-(CPL)]=='E'){ //test for "6E"
        sprintf ( Msgbuf, " (%c%s", Pgbuf[cnt-(CPL+2)], "THE)"); //"true"; Insert preceeding character plus correction "THE"
      }
      if(Pgbuf[cnt-(CPL+1)]== '6' && Pgbuf[cnt-(CPL)]=='A'){ //test for "6A"
        sprintf ( Msgbuf, " (%c%s", Pgbuf[cnt-(CPL+2)], "THA)"); //"true"; Insert preceeding character plus correction "THA"
      }
      if(Pgbuf[cnt-(CPL+1)]== '9' && Pgbuf[cnt-(CPL)]=='E'){ //test for "9E"
        sprintf ( Msgbuf,  " (ONE)" ); //"true"; Insert correction "ONE"
      }
      if(Pgbuf[cnt-(CPL+2)]=='P'&& Pgbuf[cnt-(CPL+1)]=='L' && Pgbuf[cnt-(CPL)]=='L'){ //test for "PLL"
        sprintf ( Msgbuf, " (WELL)" ); //"true"; Insert correction "WELL"
      }
      if((Pgbuf[cnt-(CPL+2)]=='N' || Pgbuf[cnt-(CPL+2)]=='L') && Pgbuf[cnt-(CPL+1)]=='M'  && Pgbuf[cnt-(CPL)]=='Y'){ //test for "NMY/LMY"
        sprintf ( Msgbuf, " (%c%s", Pgbuf[cnt-(CPL+2)], "OW)"); //"true"; Insert correction "NOW"/"LOW"
      }
      if(Pgbuf[cnt-(CPL+2)]=='T'&& Pgbuf[cnt-(CPL+1)]=='T' && Pgbuf[cnt-(CPL)]=='O'){ //test for "PD"
        sprintf ( Msgbuf, "  (0)"); //"true"; Insert correction "TTO" = "0"
      }

    }
    //recalculate maximum wait interval to splice decodeval 
    if(Bug3 && curChar != ' '){//if(Bug3 & (cnt>CPL) & (Pgbuf[cnt-(CPL)]!=' ') ){ //JMH 20200925 with current algorithm, no longer need to wait for "Pgbuf" to become active 
      //if ((ShrtBrk[LtrCntr] > 1.5*ShrtBrkA) & (ShrtBrk[LtrCntr] <3* ltrBrk)){
      //if ((ShrtBrk[LtrCntr] > 1.5*ltrBrk) & (ShrtBrk[LtrCntr] <3* ltrBrk)& (info[0] == 0)){
      //if ((ShrtBrk[LtrCntr] < 1.2*ltrBrk) & (ShrtBrk[LtrCntr]> ShrtBrkA)& (info[0] == 0)){ // this filter is based on W1AW sent code
      ltrBrk = (60*UsrLtrBrk)/100; //Jmh 20200925 added this to keep "ltrBrk" at a dynamic/reasonable value with respect to what the sender is doing
      if( ShrtBrk[LtrCntr]< UsrLtrBrk){// we're working with the last letter received was started before a normal letter break period
//        if(Pgbuf[cnt-(CPL+1)]=='T'&& Pgbuf[cnt-(CPL)]=='T'){ // we have 2 'T's in a row so increase the "ShrtFctr" a bit
//          if(ShrtBrk[LtrCntr]> ltrBrk) ShrtFctr = float(float(ShrtBrk[LtrCntr])/float(UsrLtrBrk));
//          else ShrtFctr = float(float(ltrBrk)/float(UsrLtrBrk));
        
          ShrtFctr = float(float(80*ltrBrk)/float(100*UsrLtrBrk));
          ShrtBrkA = (80*UsrLtrBrk)/100; //ShrtBrkA = (76*UsrLtrBrk)/100; //ShrtBrkA = (88*UsrLtrBrk)/100; //ShrtBrkA = (90*UsrLtrBrk)/100; //ShrtBrkA =  ShrtFctr*UsrLtrBrk;
//          AvgShrtBrk = ShrtBrkA;
          
//        }

//          ShrtFctr += 0.05;
//          if(Pgbuf[cnt-(CPL+2)]=='T') {// Whoa, Now we have 3 T's in a row, recalibrate the timing settings 
//           float ShrtFctrold = ShrtFctr;
//           ShrtFctr = float(float(ShrtBrk[LtrCntr])/float(UsrLtrBrk));
//           if(ShrtFctr < ShrtFctrold) ShrtFctr = ShrtFctrold;// the new value should have been greater than what we had been using, but not true, so "bumb up" the old value a bit es use it
//           //ShrtFctr = 2.0*float(float(AvgShrtBrk)/float(UsrLtrBrk));
//           AvgShrtBrk = ShrtBrk[LtrCntr];
//           ShrtBrkA =  ShrtFctr*UsrLtrBrk;
//          }
//        }
      }
      
      if ((ShrtBrk[LtrCntr] < 0.6*wordBrk) && curChar != ' ' && (info[0] == 0)){ //if ((ShrtBrk[LtrCntr] < 0.6*wordBrk) & (ShrtBrk[LtrCntr]> ShrtBrkA)& (info[0] == 0)){ // this filter is based on Bug sent code  
        UsrLtrBrk = (5*UsrLtrBrk+ShrtBrk[LtrCntr])/6; //(6*UsrLtrBrk+ShrtBrk[LtrCntr])/7; //(9*UsrLtrBrk+ShrtBrk[LtrCntr])/10;
        //ShrtBrkA =  ShrtFctr*UsrLtrBrk;//0.45*UsrLtrBrk; 
      //} else if((info[0] != 0)&(ShrtBrk[LtrCntr]<ShrtBrkA/2) ){// we just processed a spliced character
      } //else if((info[0] == '*')){// we just processed a spliced character

      //}

    }
    if(Bug3 && SCD && Test){
      char info1[50];
      sprintf(info1, "{%s}\t%s",DeBugMsg, info);
      if(info[0] == '*') info[0] = '^';//change the info to make it recognizable when the replacement characters are part the same group
      char str_ShrtFctr[6];
      sprintf(str_ShrtFctr,"%d.%d", int(ShrtFctr), int(1000*ShrtFctr));
      char Ltr;
      if(cnt < CPL){
        Ltr = curChar;
      }else Ltr = Pgbuf[cnt-(CPL)];
      sprintf(DeBugMsg, "%d %d \t%c%c%c %d/%d/%d/%d/%s   \t%d/%d\t  \t%d\t%d \t%s ",LtrCntr, ShrtBrk[LtrCntr],'"', Ltr,'"', ShrtBrkA, ltrBrk, wordBrk, UsrLtrBrk, str_ShrtFctr, cursorX, cursorY,  cnt, xoffset, info1);
      Serial.println(DeBugMsg);
      //LtrCntr = 0;
    }
    OldLtrPntr = LtrCntr;
    int i = 0;
    while((DeBugMsg[i] != 0) && (i<150) ){
      DeBugMsg[i] = 0;
      i++;
    }
    msgpntr++;
    cnt++;
    if ((cnt - offset)*fontW >= displayW) {
      curRow++;
      //newRow = true;
      cursorX = 0;
      cursorY = curRow * (fontH + 10);
      offset = cnt;
      tft.setCursor(cursorX, cursorY);
      if (curRow + 1 > row) {
        scrollpg();
      }
    }
    else{
      cursorX = (cnt - offset) * fontW;
      //newRow = false;
    }
    
  }
  LtrCntr = 0;
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
  //Serial.println(ModeCnt);
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
    case 3:
      tft.print("Bug3");
      break;  
    default:
      tft.print(ModeCnt);
      break;
  }
}
///////////////////////////////////////////////////////////


void showSpeed(){
  char buf[20];
  int ratioInt = (int)curRatio;
  int ratioDecml = (int)((curRatio - ratioInt) * 10);
  chkStatsMode = true;
  //if (SwMode && buttonEnabled) SwMode = false;
  switch (statsMode){
  case 0:
    sprintf ( buf, "%d/%d.%d WPM", wpm, ratioInt, ratioDecml);
    break;
  case 1:
    sprintf ( buf, "%d", avgDit);
    sprintf ( buf, "%s/%d", buf, avgDah);
    sprintf ( buf, "%s/%d", buf, avgDeadSpace );
    break;
  case 2:
    sprintf ( buf, "FREQ: %dHz", int(TARGET_FREQUENCYC));
    break;
  case 3:
    ratioInt = (int)SNR;
    ratioDecml = (int)((SNR - ratioInt) * 10);
    sprintf ( buf, "SNR: %d.%d/1", ratioInt, ratioDecml);
    break;  
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
