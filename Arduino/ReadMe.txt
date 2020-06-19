The files found here are designed to be compiled and loaded using the Arduino ide.
The STM32_CWdecoderW_ToneDetectR03 folder/sketch is the sketch to use if you are following the instructions
from the video verbatum. i.e. 64K bluepill & MCUfriends compatible 3.5" display sheild.

The MapleMini_CWdecoderW_ToneDetectR00 folder/sketch should work with 128k STM32f MCUs (e.g.; Maple Mini).
This sketch will work with the MCUfriends 3.5" display. Additionally it can also configured to work with some variants 
of the MCUfriends display, that use the same parallel data bus. 

To test and configure the MapleMini_CWdecoderW_ToneDetect sketch, two other support sketches have been added (which are not described in the video).

The "STM32_diagnose_TFT_support" folder/sketch can be compiled and loaded on to the maple mini, and via the ide's serial monitor tool (and possibly via the display) will report the display's basic compatibility with the libraries used in this project.

Assuming that your display reports to be compatible using the above sketch, then the "STM32_TouchScreen_Calibr_native" sketch
can be compiled and loaded on to your stm32f board (maple mini). Following the instrustions presented on the display. 
This sketch will generate a report (on the serial monitor tool) that can be copied and pasted into the 
"MapleMini_CWdecoderW_ToneDetect" sketch, to set the needed parameters (variable assignments) to configure the touch screen.

specifically these:

	const int XP=PB5,XM=PA7,YP=PA6,YM=PB6; //480x320 ID=0x9090
	const int TS_LEFT=939,TS_RT=95,TS_TOP=906,TS_BOT=106; //

	uint16_t ID = 0x9090; // Touch Screen ID obtained from either STM32_TouchScreen_Calibr_native or BluePillClock sketch

If after loading the reported parameters, your display's touch points are NOT aligned with the buttons, try swapping the
TS_TOP and TS_BOT values.    
