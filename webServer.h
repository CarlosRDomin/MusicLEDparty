/******      Web Server      ******/
#ifndef WEB_SERVER_H_
#define WEB_SERVER_H_

#include "main.h"						// HotTub global includes and definitions
#include "WiFi.h"						// WiFi library needed for secret settings (to detect which interface a client is connected on, WLAN or AP)
#include "GPIO.h"						// GPIO library so HTTP server can change IO state upon request
#include <ESPAsyncWebServer.h>			// (Secret) HTTP web server
#include <SPIFFSEditor.h>				// Helper that provides the resources to view&edit SPIFFS files through HTTP
#include <ESP8266HTTPUpdateServer.h>	// OTA (upload firmware through HTTP browser over WiFi)
#include <WebSocketsServer.h>			// WebSockets

#define consolePrintF(s, ...)		consolePrintf(String(F(s)).c_str(), ##__VA_ARGS__)
#define UNIQUE_HOSTNAME				false	// If true, use ESP.getChipId() to create a unique hostname; Otherwise, use "CarlitosHotTub"
#define USE_MDNS					false
#define USE_ARDUINO_OTA				false
#define ARDUINO_OTA_PORT			8266	// Puerto que reconoce la IDE de Arduino y permite flashear firmware desde el IDE
#define SECRET_SERVER_USER			ARDUINO_OTA_USER
#define SECRET_SERVER_PASS			ARDUINO_OTA_PASS
#define MAKE_SECRET_SETTS_PUBLIC	true
#define PORT_PUBLIC_SETTS			80		// Port for public web server
#define PORT_WEBSOCKET_FFT			81		// Port for the webSocket for FFT debugging purposes
#define PORT_WEBSOCKET_CONSOLE		82		// Port for the webSocket to which debug Serial.print messages are forwarded


#define CONT(x)						String(FPSTR(contentType_P[x]))	// Helper macro to specify a MIME content type as a String from a PROGMEM copy
#define UPLOAD_TEMP_FILENAME		"/tmp.file"	// Temporary file name given to a file uploaded through the web server. Once we receive its desired path, we'll rename it (move it)

#if USE_MDNS
#include <ESP8266mDNS.h>			// DNS (permite asignar un dominio para no necesitar saber IP)
#endif

#if USE_ARDUINO_OTA
#include <ArduinoOTA.h>				// OTA (upload firmware through Arduino IDE over WiFi)
#endif

/*extern AsyncWebServer serverSecret;
extern ESP8266HTTPUpdateServer server_OTA_uploader;*/
extern WebSocketsServer webSocketConsole;

enum {TYPE_PLAIN=0, TYPE_HTML, TYPE_JSON, TYPE_CSS, TYPE_JS, TYPE_PNG, TYPE_GIF, TYPE_JPG, TYPE_ICO, TYPE_XML, TYPE_PDF, TYPE_ZIP, TYPE_GZ, TYPE_DLOAD};
const char* const PROGMEM contentType_P[] = {"text/plain", "text/html", "text/json", "text/css", "application/javascript", "image/png", "image/gif", "image/jpeg", "image/x-icon", "text/xml", "application/x-pdf", "application/x-zip", "application/x-gzip", "application/octet-stream"};


/***************************************************/
/******            SETUP FUNCTIONS            ******/
/***************************************************/
void setupWebServer();	// Initialize hostName, mDNS, web server, OTA methods (HTTP-based and IDE-based) and webSockets


/****************************************************/
/******      Web server related functions      ******/
/****************************************************/
void addNoCacheHeaders(AsyncWebServerResponse* response);	// Add specific headers to an http response to avoid caching
/*bool isInterfaceAP(AsyncWebServer& server);		// Returns true if client is connected through my own AP; false if connected on same WLAN
bool ensureSecretSettingsVisibility(AsyncWebServer& server);*/	// Makes sure secret settings can only be accessed from the AP network (not through WLAN). Returns true if client has permission to see the settings.
/*void sendRedirect(String newUri, AsyncWebServer& server);	// Send an HTTP 302 response so user is redirected to the appropriate web page
void handleFileUpload(AsyncWebServer& server);	// Allows user to upload a file to the SPIFFS (so we don't have to write the whole Flash via USB)
bool renameFileUpload(String fileName);*/	// Renames the last file uploaded to the new file name provided
//void secretSettingsWLANedit(AsyncWebServerRequest* request);	// Handles secret HTTP page that allows me to change WLAN settings
void secretSettingsWLANscan(AsyncWebServerRequest* request);	// Handles secret HTTP page that scans WLAN networks
void secretSettingsWLANsave(AsyncWebServerRequest* request);	// Handles secret HTTP page that saves new WLAN settings
void secretSettingsListLEDeffects(AsyncWebServerRequest* request);	// Lists all config files related to LED strip effects
void webSocketFFTevent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght);	// webSocketFFT event callback function
void webSocketConsoleEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght);	// webSocketConsole event callback function
void consolePrintf(const char * format, ...);	// Log messages through webSocketConsole and Serial
int constexpr precompute_strlen(const char* str);
void processWebServer();	// "secretSettings.loop()" function: handle incoming OTA connections (if any), secret settings http requests and webSocket events

#endif

