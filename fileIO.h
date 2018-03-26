/******      File IO helpers      ******/
#ifndef PUBLIC_SETTINGS_H_
#define PUBLIC_SETTINGS_H_

#include "main.h"						// HotTub global includes and definitions
#include <FS.h>							// SPIFFS file system (to read/write to flash)
#include <ArduinoJson.h>				// JSON parser library (to load/save settings, read files for the web server, etc)

#define JSON_BUFFER_SIZE		512


/***************************************************/
/******            SETUP FUNCTIONS            ******/
/***************************************************/
void setupFileIO();	// Initializes file system, so we can read/write config and web files


/*************************************************/
/******      File IO related functions      ******/
/*************************************************/
std::unique_ptr<char[]> readFile(String filePath);	// Reads the contents of a file and returns a pointer to a dynamically allocated char[] buffer. Pointer will evaluate to NULL on fail.
bool saveJSON(JsonObject& json, String filePath);	// Saves the contents of a json buffer to a file, and returns whether we succeeded or not

#endif

