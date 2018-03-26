/******      OLED display      ******/
#include "OLED.h"

#if USE_OLED_DISP
Adafruit_SSD1306 display(-1);		// If we pass a number as the first parameter, it would be used as the OLED_RESET pin (-1 for no reset)
#endif


/***************************************************/
/******            SETUP FUNCTIONS            ******/
/***************************************************/
void setupOLEDdisplay() {	// Initialize OLED display (if USE_OLED_DISP == true)
	#if USE_OLED_DISP
		display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
		/*display.display();
		delay(2);*/
		display.clearDisplay();
		display.setTextSize(1);
		display.setTextColor(WHITE);
		display.setCursor(0,0);
		display.println(SF("Conectando WiFiMulti"));
		display.display();
	#endif
}


/******************************************************/
/******      OLED display related functions      ******/
/******************************************************/
void oledDrawVLineFromBottom(uint8_t x, uint8_t h, uint16_t color) {	// Helper function to draw a vertical line from the bottom edge at coordinate x and height y
	#if USE_OLED_DISP
		if (h > SSD1306_LCDHEIGHT) {
			h = SSD1306_LCDHEIGHT;
		}
		display.drawFastVLine(x,SSD1306_LCDHEIGHT-h, h, color);
	#endif
}

void processOLED() {	// "OLED.loop()" function: turn the screen into an audio spectrum analyzer (histogram of each freq bin's power)
	#if USE_OLED_DISP
		display.clearDisplay();
		
		for (uint16_t i=0; i<=N_FFT/2; ++i) {
			oledDrawVLineFromBottom(i, fft_magn[i]/4000*SSD1306_LCDHEIGHT);
		}
		
		display.display();
	#endif
}

