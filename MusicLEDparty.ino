/******      MusicLEDparty main file      ******/
#include "main.h"
#include "GPIO.h"
#include "FFT.h"
#include "OLED.h"
#include "fileIO.h"
#include "WiFi.h"
#include "webServer.h"
#include "ledStrip.h"

uint32_t curr_time;


/***************************************************/
/******            SETUP FUNCTIONS            ******/
/***************************************************/
void setup() {
	curr_time = millis();
	Serial.begin(115200);
	Serial.setDebugOutput(true);	// Print debug messages through Serial (for debugging)
	while(!Serial);	// Wait for serial port to connect

	setupIOpins();
	setupOLEDdisplay();
	setupWiFi();
	setupFileIO();
	setupWebServer();
	setupLedStrip();
}


/*********************************************/
/******            MAIN LOOP            ******/
/*********************************************/
void loop() {
	curr_time = millis();
	
	processGPIO();
	processOLED();
	processLedStrip();
	processWebServer();
	processWiFi();

	delay(10);
}

