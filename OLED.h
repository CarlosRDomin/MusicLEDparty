/******      OLED display      ******/
#ifndef OLED_H_
#define OLED_H_

#include "main.h"						// HotTub global includes and definitions
#include "FFT.h"						// FFT library so we can draw a histogram of current sound on the screen

#define USE_OLED_DISP	false

#if USE_OLED_DISP
#include <Adafruit_SSD1306.h>			// OLED display
extern Adafruit_SSD1306 display;
#endif


/***************************************************/
/******            SETUP FUNCTIONS            ******/
/***************************************************/
void setupOLEDdisplay();	// Initialize OLED display (if USE_OLED_DISP == true)


/******************************************************/
/******      OLED display related functions      ******/
/******************************************************/
void oledDrawVLineFromBottom(uint8_t x, uint8_t h, uint16_t color = 1);	// Helper function to draw a vertical line from the bottom edge at coordinate x and height y
void processOLED();	// "OLED.loop()" function: turn the screen into an audio spectrum analyzer (histogram of each freq bin's power)

#endif

