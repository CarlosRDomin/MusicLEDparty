/******      HotTub main include      ******/
#ifndef MAIN_H_
#define MAIN_H_

#include <Arduino.h>			// Aruino general includes and definitions
#include "secretDefines.h"		// Defines passwords, etc: CONNECT_TO_SSID, CONNECT_TO_PASS, SOFT_AP_SSID, SOFT_AP_PASS, ARDUINO_OTA_USER, ARDUINO_OTA_PASS, SECRET_SERVER_PORT
#include "webServer.h"			// Web server library in case we want to print debug messages through a webSocket (consolePrintF method)
#include "OLED.h"				// OLED library in case we want to print messages to the OLED display

#define SF(literal)		String(F(literal))			// Macro to save a string literal in Flash memory and convert it to String when reading it
#define CF(literal)		String(F(literal)).c_str()	// Macro to save a string literal in Flash memory and convert it to char* when reading it
#define SFPSTR(progmem)	String(FPSTR(progmem))		// Macro to convert a PROGMEM string (in Flash memory) to String

extern uint32_t curr_time;

#endif

