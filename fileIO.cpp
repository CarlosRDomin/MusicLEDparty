/******      File IO helpers      ******/
#include "fileIO.h"


/***************************************************/
/******            SETUP FUNCTIONS            ******/
/***************************************************/
void setupFileIO() {	// Initializes file system, so we can read/write config and web files
	if (!SPIFFS.begin()) {
		consolePrintF("Failed to mount file system!!\n");
	} else {
		consolePrintF("SPIFFS loaded!\n");
	}
}


/*************************************************/
/******      File IO related functions      ******/
/*************************************************/
std::unique_ptr<char[]> readFile(String filePath) {	// Reads the contents of a file and returns a pointer to a dynamically allocated char[] buffer. Pointer will evaluate to NULL on fail.
	File f = SPIFFS.open(filePath, "r");
	if (!f) {
		consolePrintF("Failed to open SPIFFS file %s :(\n", filePath.c_str());
		return std::unique_ptr<char[]>{};
	}
	
	size_t size = f.size();
	if (size > 2048) {
		consolePrintF("SPIFFS file %s is too large (%d B), can't open :(\n", filePath.c_str(), size);
		return std::unique_ptr<char[]>{};
	}
	
	std::unique_ptr<char[]> buf(new char[size]);	// Allocate a buffer to store contents of the file. unique_ptr ensures delete[] gets called when the buffer gets out of scope :)
	f.readBytes(buf.get(), size);	// ArduinoJson library requires the input buffer to be mutable, so we gotta use char[] instead of String
	consolePrintF("Successfully read SPIFFS file %s!\n", filePath.c_str());
	return buf;
}

bool saveJSON(JsonObject& json, String filePath) {	// Saves the contents of a json buffer to a file, and returns whether we succeeded or not
	File f = SPIFFS.open(filePath, "w");
	if (!f) {
		consolePrintF("Failed to save JSON file %s :(\n", filePath.c_str());
		return false;
	}
	
	json.printTo(f);
	consolePrintF("Successfully saved JSON file %s!\n", filePath.c_str());
	return true;
}

