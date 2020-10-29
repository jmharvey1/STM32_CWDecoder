/*
     The TFTLCD library comes from here:  https://github.com/prenticedavid/MCUFRIEND_kbv
      note: after inporting the above library using the ide add zip library menu option, remane the folder to remover the word "master"
  ^   The Touch Screen & GFX libraries are standard (unmodified) Adafruit libraries & were found here:4
         https://github.com/adafruit/Adafruit-GFX-Library
         https://github.com/adafruit/Touch-Screen-Library
*/
/*
 * JMH 20200127 Modified Skecth to Run from STM32F103C8 timer based interrupt
*/

#include "Adafruit_GFX.h"
//#include <gfxfont.h>
//#include <MCUFRIEND_kbv.h>
#include "MCUFRIEND_kbv.h"
// MCU Friend TFT Display to STM32F pin connections
//LCD        pin |D7 |D6 |D5 |D4 |D3 |D2 |D1 |D0 | |RD  |WR |RS |CS |RST | |SD_SS|SD_DI|SD_DO|SD_SCK|
//Blue Pill  pin |PA7|PA6|PA5|PA4|PA3|PA2|PA1|PA0| |PB0 |PB6|PB7|PB8|PB9 | |PA15 |PB5  |PB4  |PB3   | **ALT-SPI1**
//Maple Mini pin |PA7|PA6|PA5|PA4|PA3|PA2|PA1|PA0| |PB10|PB6|PB7|PB5|PB11| |PA15 |PB5  |PB4  |PB3   | //Mini Mapl

#if defined(ARDUINO_MAPLE_MINI)
#define LED_BUILTIN PB1   //Maple Mini
#define MicPin PB0
#else
#define LED_BUILTIN PC13  //Blue Pill Pin
#define MicPin PB1
#endif
#define TIC_RATE 1000000    // in microseconds; should give 1.0Hz toggles
MCUFRIEND_kbv tft;

unsigned long AvgBias =0;
uint16_t ID;
void Timer_ISR(void);
int toggle = 0;

float sx = 0, sy = 1, mx = 1, my = 0, hx = -1, hy = 0;    // Saved H, M, S x & y multipliers
float sdeg=0, mdeg=0, hdeg=0;
uint16_t osx=120, osy=120, omx=120, omy=120, ohx=120, ohy=120;  // Saved H, M, S x & y coords
uint16_t x00=0, x11=0, y00=0, y11=0;
//uint32_t targetTime = 0;                    // for next 1 second timeout
boolean initial = 1;
char Bufr[14];

static uint8_t conv2d(const char* p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9')
    v = *p - '0';
  return 10 * v + *++p - '0';
}

uint8_t hh=conv2d(__TIME__), mm=conv2d(__TIME__+3), ss=conv2d(__TIME__+6);  // Get H, M, S from compile time


void setup(void) {
  pinMode(PB1, INPUT);
  tft.reset();
  
  ID = tft.readID();
  
  //  ID = 0x4747;//MCUfriends.com 2.8" Display id
  //  ID = 0x9486;// MCUfriends.com 2.8" Display id;
  //  ID = 0x9341;// MCUfriends.com 2.8" Display id; (White Letters on Black Screen)
  //  ID = 0x6814;// MCUfriends.com 3.5" Display id
  //  ID = 0x9090;//MCUfriends.com 3.5" Display id (Black Letters on White Screen)
  //if(ID == 0x9090) ID = 0x9341; //do this to keep the correct color scheme
  tft.begin(ID);//  The value here is screen specific & depends on the chipset used to drive the screen,
  tft.setRotation(2);
  
  //tft.fillScreen(ILI9341_BLACK);
  //tft.fillScreen(ILI9341_RED);
  //tft.fillScreen(ILI9341_GREEN);
  //tft.fillScreen(ILI9341_BLUE);
  //tft.fillScreen(ILI9341_BLACK);
  //tft.fillScreen(ILI9341_GREY);
  tft.fillScreen(TFT_LIGHTGREY);
 
  
  //tft.setTextColor(ILI9341_WHITE, ILI9341_GREY);  // Adding a background colour erases previous text automatically
  //tft.setTextColor(TFT_WHITE, ILI9341_GREY);  // Adding a background colour erases previous text automatically
  tft.setTextColor(TFT_WHITE, TFT_LIGHTGREY);  // Adding a background colour erases previous text automatically
  
  // Draw clock face
  //tft.fillCircle(120, 120, 118, ILI9341_GREEN);
  //tft.fillCircle(120, 120, 110, ILI9341_BLACK);
  tft.fillCircle(120, 120, 118, TFT_GREEN);
  tft.fillCircle(120, 120, 110, TFT_BLACK);

  // Draw 12 lines
  for(int i = 0; i<360; i+= 30) {
    sx = cos((i-90)*0.0174532925);
    sy = sin((i-90)*0.0174532925);
    x00 = sx*114+120;
    y00 = sy*114+120;
    x11 = sx*100+120;
    y11 = sy*100+120;

    //tft.drawLine(x00, y00, x11, y11, ILI9341_GREEN);
    tft.drawLine(x00, y00, x11, y11, TFT_GREEN);
  }

  // Draw 60 dots
  for(int i = 0; i<360; i+= 6) {
    sx = cos((i-90)*0.0174532925);
    sy = sin((i-90)*0.0174532925);
    x00 = sx*102+120;
    y00 = sy*102+120;
    // Draw minute markers
    //tft.drawPixel(x00, y00, ILI9341_WHITE);
    tft.drawPixel(x00, y00, TFT_WHITE);
    
    // Draw main quadrant dots
    //if(i==0 || i==180) tft.fillCircle(x00, y00, 2, ILI9341_WHITE);
    //if(i==90 || i==270) tft.fillCircle(x00, y00, 2, ILI9341_WHITE);
    if(i==0 || i==180) tft.fillCircle(x00, y00, 2, TFT_WHITE);
    if(i==90 || i==270) tft.fillCircle(x00, y00, 2, TFT_WHITE);
  }

  //tft.fillCircle(120, 121, 3, ILI9341_WHITE);
  tft.fillCircle(120, 121, 3, TFT_WHITE);

  // Draw text at position 120,260 using fonts 4
  // Only font numbers 2,4,6,7 are valid. Font 6 only contains characters [space] 0 1 2 3 4 5 6 7 8 9 : . a p m
  // Font 7 is a 7 segment font and only contains characters [space] 0 1 2 3 4 5 6 7 8 9 : .
  dispMsg(" Time Flies", 20, 260, 3);
  sprintf(Bufr," ID = 0x%x", ID );
  dispMsg(Bufr, 20, 290, 3);

  //targetTime = millis() + 1000;
  
  // initialize digital pin PB1 as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  
  //Timer Interrupt stuff
  // Setup ISR Timer (in this case Timer2)
  Timer2.setMode(TIMER_CH1, TIMER_OUTPUTCOMPARE);
  Timer2.setPeriod(TIC_RATE); // in microseconds
  Timer2.setCompare(TIMER_CH1, 1);      // overflow might be small
  Timer2.attachInterrupt(TIMER_CH1, Timer_ISR);
   
}

void loop() {
  Serial.print("Program Running"); 
  while(1){
    delay(20);
    int k = analogRead(MicPin);    // read the ADC value from pin PB1; (k = 0 to 4096)
    AvgBias = ((99* AvgBias) +k)/100;
  }
}
//////////////////////////////////////////////////////////////////////

void Timer_ISR(void) {
    toggle ^= 1;
    digitalWrite(LED_BUILTIN, toggle);
    ss++;              // Advance second
    if (ss==60) {
      ss=0;
      mm++;            // Advance minute
      if(mm>59) {
        mm=0;
        hh++;          // Advance hour
        if (hh>23) {
          hh=0;
        }
      }
    }

    // Pre-compute hand degrees, x & y coords for a fast screen update
    sdeg = ss*6;                  // 0-59 -> 0-354
    mdeg = mm*6+sdeg*0.01666667;  // 0-59 -> 0-360 - includes seconds
    hdeg = hh*30+mdeg*0.0833333;  // 0-11 -> 0-360 - includes minutes and seconds
    hx = cos((hdeg-90)*0.0174532925);    
    hy = sin((hdeg-90)*0.0174532925);
    mx = cos((mdeg-90)*0.0174532925);    
    my = sin((mdeg-90)*0.0174532925);
    sx = cos((sdeg-90)*0.0174532925);    
    sy = sin((sdeg-90)*0.0174532925);

    if (ss==0 || initial) {
      initial = 0;
      // Erase hour and minute hand positions every minute
      //tft.drawLine(ohx, ohy, 120, 121, ILI9341_BLACK);
      tft.drawLine(ohx, ohy, 120, 121, TFT_BLACK);
      ohx = hx*62+121;    
      ohy = hy*62+121;
      //tft.drawLine(omx, omy, 120, 121, ILI9341_BLACK);
      tft.drawLine(omx, omy, 120, 121, TFT_BLACK);
      omx = mx*84+120;    
      omy = my*84+121;
    }

      // Redraw new hand positions, hour and minute hands not erased here to avoid flicker
      //tft.drawLine(osx, osy, 120, 121, ILI9341_BLACK);
      tft.drawLine(osx, osy, 120, 121, TFT_BLACK);
      osx = sx*90+121;    
      osy = sy*90+121;
      tft.drawLine(osx, osy, 120, 121, TFT_RED);
      tft.drawLine(ohx, ohy, 120, 121, TFT_WHITE);
      tft.drawLine(omx, omy, 120, 121, TFT_WHITE);
      tft.drawLine(osx, osy, 120, 121, TFT_RED);

    //tft.fillCircle(120, 121, 3, ILI9341_RED);
    tft.fillCircle(120, 121, 3, TFT_RED);
    sprintf(Bufr," Bias: %d", AvgBias );
    dispMsg(Bufr, 20, 320, 3);
} 
//////////////////////////////////////////////////////////////////////

void dispMsg(char Msgbuf[32], int cursorX, int cursorY, int FontSz) {
  int msgpntr = 0;
  int curRow = 0;
  int offset =0;
  int fontH = 16;
  int fontW = 18;//12;
  int displayW = 320;
  int cnt = 0;
  while (Msgbuf[msgpntr] != 0) ++msgpntr;
  
  //clear previous entry
  tft.fillRect(cursorX , (cursorY), msgpntr * fontW, fontH + 10, TFT_LIGHTGREY);
  tft.setCursor(cursorX, cursorY);
  tft.setTextColor(TFT_BLACK);//tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(FontSz);
  msgpntr = 0;
  while ( Msgbuf[msgpntr] != 0) {
    char curChar = Msgbuf[msgpntr];
    tft.print(curChar);
    msgpntr++;
    cnt++;
    if (((cnt) - offset)*fontW >= displayW) {
      curRow++;
      cursorX = 0;
      cursorY = curRow * (fontH + 10);
      offset = cnt;
      tft.setCursor(cursorX, cursorY);
//      if (curRow + 1 > row) {
//        scrollpg();
//      }
    }
    else cursorX = (cnt - offset) * fontW;
  }
}
//////////////////////////////////////////////////////////////////////
