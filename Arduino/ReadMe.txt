The files found here are designed to be compiled and loaded using the Arduino ide.
The STM32_CWdecoderW_ToneDetectR03 folder/sketch is the sketch to use if you are following the instructions
from the video verbatum. i.e. 64K bluepill & MCUfriends compatible 3.5" display sheild.

The MapleMini_CWdecoderW_ToneDetectR01 folder/sketch should work with 128k STM32f MCUs (e.g.; Maple Mini).
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


Operational Notes related to the MapleMini_CWdecoderW_ToneDetectR01 only:

There is a “setup” mode that can be invoked with a “long press” of the blue “Clear” button.

In the setup mode, the user can adjust & save the following parameters:

1. Bias (+/-) ; value to subtract from ADC sample to remove the microphone's DC offset
2. MSQL(+/); Squelch value to use when tone detector is operating in MAN SQLCH mode 
3. TSF(+/-);  “Tone Scale Factor”; percentage of tone signal to subtract from noise  
4. NSF(+/-); “Noise Scale Factor”; speaker dependent; Adjust for best tone detection.
5. LED(+/-); Set maximum LED brightness.
6. Freq(+/-); tone filter center frequency to use when operating in FREQ LOCKED mode
7. Squelch Mode (NOISE SQLCH / MAN SQLCH); no explanation needed
8. Factory Vals; return decoder to sketch default value
9. Debug Mode (OFF / Plot / Decode); When not OFF, use Arduino IDE plot /serial monitor tools
10. Tone mode (AUTO Tune / FREQ LOCKED)
The setup screen has two additional buttons:
1. Exit (leave the Setup mode; return to Decode mode)
2. Save (Store the current setting to Flash memory)

When the Decoder is running in decode mode, the lower left segment of the display is touch sensitive
and will display one of three attributes:
1. current Words per Minute / Dot to Dash ratio
2. current Average Dot / Dash / Space interval (milliseconds)
3. current tone filter center frequency (hertz)
When the Decoder is running in decode mode, the Green button supports 4 modes
1. Norm; “Normal”; should work for 90% of the messages decoded
2. Bug1; Use with senders that exaggerate the spacing between dashes in the same letter
3. Bug2; Same as “Bug1” but using a different strategy to recover/decode the letter
4. Bug3; Similar to “Bug2” but uses different timing percentages to recover/decode the letter 

  
