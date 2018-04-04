/******      Web Server      ******/
#include "webServer.h"
#include "ledStrip.h"					// LED strip library needed to show config files associated with the effect list. Have to include it in the cpp file or else circular import errors are hard to deal with

char hostName[32];
AsyncWebServer serverPublic(PORT_PUBLIC_SETTS), serverSecret(SECRET_SERVER_PORT);
ESP8266HTTPUpdateServer server_OTA_uploader;
WebSocketsServer webSocketConsole(PORT_WEBSOCKET_CONSOLE), webSocketFFT(PORT_WEBSOCKET_FFT);
bool shouldReboot = false;


/***************************************************/
/******            SETUP FUNCTIONS            ******/
/***************************************************/
void setupWebServer() {	// Initialize hostName, mDNS, web server, OTA methods (HTTP-based and IDE-based) and webSockets
	// Initialize hostName
	if (UNIQUE_HOSTNAME) {
		sprintf(hostName, "Carlitos8266-%06X", ESP.getChipId());
	} else {
		sprintf(hostName, "CarlitosHotTub");
	}
	#if USE_OLED_DISP
		display.println(hostName);
		display.display();
	#endif

	// Setup mDNS so we don't need to know its IP
	#if USE_MDNS
		if (MDNS.begin(hostName)) {
			consolePrintF("mDNS arrancado, ahora tambien te puedes referir a mi como '%s.local'\n", hostName);
			// MDNS.addService(F("http"), F("tcp"), SECRET_SERVER_PORT);	// Don't want to advertise OTA's port, so only I know it :)
			MDNS.addService(F("http"), F("tcp"), PORT_PUBLIC_SETTS);
			MDNS.addService(F("ws"), F("tcp"), PORT_WEBSOCKET_FFT);
			MDNS.addService(F("ws"), F("tcp"), PORT_WEBSOCKET_CONSOLE);
		} else {
			consolePrintF("Unable to load mDNS! :(\n");
		}
	#endif
	
	// Start servers and websockets
	serverPublic.on(SF("/imgHotTubState").c_str(), HTTP_GET, [](AsyncWebServerRequest* request) { request->redirect(SF("/img/HotTub_l") + (lightsOn? F("On"):F("Off")) + F("_s") + (soundOn? F("On"):F("Off")) + F(".png")); /*serverPublic.sendHeader(F("Cache-Control"), F("must-revalidate")); serveFile(SF("/img/HotTub_l") + (lightsOn? F("On"):F("Off")) + F("_s") + (soundOn? F("On"):F("Off")) + F(".png"), CONT(TYPE_PNG), serverPublic);*/ });
	serverPublic.on(SF("/txtHotTubState").c_str(), HTTP_GET, [](AsyncWebServerRequest* request){ request->send(200, CONT(TYPE_JSON), SF("{\"lights\":") + lightsOn + F(",\"sound\":") + soundOn + F("}")); });
	serverPublic.on(SF("/on").c_str(), HTTP_GET, [](AsyncWebServerRequest* request){ turnRelays(true, request); });
	serverPublic.on(SF("/off").c_str(), HTTP_GET,  [](AsyncWebServerRequest* request){ turnRelays(false, request); });
	serverPublic.on(SF("/lights_on").c_str(), HTTP_GET, [](AsyncWebServerRequest* request){ turnLights(true, request); });
	serverPublic.on(SF("/lights_off").c_str(), HTTP_GET, [](AsyncWebServerRequest* request){ turnLights(false, request); });
	serverPublic.on(SF("/lights_toggle").c_str(), HTTP_GET, [](AsyncWebServerRequest* request){ turnLights(!lightsOn, request); });
	serverPublic.on(SF("/sound_on").c_str(), HTTP_GET, [](AsyncWebServerRequest* request){ turnSound(true, request); });
	serverPublic.on(SF("/sound_off").c_str(), HTTP_GET,  [](AsyncWebServerRequest* request){ turnSound(false, request); });
	serverPublic.on(SF("/sound_toggle").c_str(), HTTP_GET, [](AsyncWebServerRequest* request){ turnSound(!soundOn, request); });
	serverPublic.serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html").setCacheControl("public, max-age=1209600");	// Cache for 2 weeks :)
	serverPublic.onNotFound([](AsyncWebServerRequest* request) { request->send(404, CONT(TYPE_PLAIN), SF("Not found: ") + request->url()); });
	
	serverPublic.begin();

	// Configure HTTP server for secret settings (eg, firmware upload on http://hostName:SECRET_SERVER_PORT/updateFirmware, etc.)
//	server_OTA_uploader.setup(&serverPublic, SF("/updateFirmware").c_str(), ARDUINO_OTA_USER, ARDUINO_OTA_PASS);	// ATENCION: serverSecret
//	serverSecret.on(SF("/WiFi").c_str(), HTTP_GET, [](AsyncWebServerRequest* request) { request->send(SPIFFS, "/WiFi.html"); });
	serverSecret.rewrite("/WiFi", "/WiFi.html");
	serverSecret.on(SF("/WiFiNets").c_str(), HTTP_GET, secretSettingsWLANscan);
	serverSecret.on(SF("/WiFiSave").c_str(), HTTP_POST, secretSettingsWLANsave);
	serverSecret.on(SF("/listEffects").c_str(), HTTP_GET, secretSettingsListLEDeffects);
	serverSecret.on(SF("/heap").c_str(), HTTP_GET, [](AsyncWebServerRequest* request) { AsyncWebServerResponse* response = request->beginResponse(200, CONT(TYPE_PLAIN), String(ESP.getFreeHeap()) + F(" B")); addNoCacheHeaders(response); response->addHeader(F("Refresh"), F("2")); request->send(response); });

	serverSecret.on("/secretOTA", HTTP_GET, [](AsyncWebServerRequest* request) {
		request->send(200, CONT(TYPE_HTML), F("<form method='POST' action='/secretOTA' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>"));
	});
	serverSecret.on("/secretOTA", HTTP_POST, [](AsyncWebServerRequest* request) {
		shouldReboot = !Update.hasError();
		AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot? "Succssfully got new firmware! Restarting...":"Something went wrong uploading the firmware, sorry try again :(");
		addNoCacheHeaders(response);
		response->addHeader(F("Refresh"), F("15; url=/heap"));
		request->send(response);
	}, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
		if (!index) {
			consolePrintF("\n\t---> OTA update start! %s\n", filename.c_str());
			Update.runAsync(true);
			if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
				Update.printError(Serial);
			}
		}
		if (!Update.hasError()) {
			if (Update.write(data, len) != len) {
				Update.printError(Serial);
			}
		}
		if (final) {
			if (Update.end(true)) {
				consolePrintF("\n\t---> Successful OTA upload: %uB!\n", index+len);
			} else {
				Update.printError(Serial);
			}
		}
	});
	
	/*serverSecret.on(SF("/secretUpload").c_str(), HTTP_GET, [](AsyncWebServerRequest* request){ if (SPIFFS.exists(F("/secretUpload.html"))) { request->send(SPIFFS, F("/secretUpload.html")); } else { request->send(200, contentType_P[TYPE_HTML], F("<html><head><link rel=\"stylesheet\" href=\"css/styles.css\"></head><body><h1>Secret file uploader</h1><form method=\"POST\" action=\"secretUpload\" enctype=\"multipart/form-data\"><p>New file name: <input type=\"text\" placeholder=\"/\" name=\"fileName\" value=\"\" /><br><input type=\"file\" name=\"fileContent\" value=\"\" /><br><input type=\"submit\" value=\"Upload file\" /></p></form></body></html>")); } });
	serverSecret.on(SF("/secretUpload").c_str(), HTTP_POST, [](AsyncWebServerRequest* request){ bool ok = renameFileUpload(serverSecret.arg(F("fileName"))); sendRedirect(SF("secretUpload?f=") + serverSecret.arg(F("fileName")) + F("&ok=") + ok, serverSecret); }, [](){ handleFileUpload(serverSecret); });*/
	serverSecret.on(SF("/secretRestart").c_str(), HTTP_GET, [](AsyncWebServerRequest* request) { AsyncWebServerResponse* response = request->beginResponse(200, contentType_P[TYPE_PLAIN], F("Restarting!")); addNoCacheHeaders(response); response->addHeader(F("Refresh"), F("15; url=/heap")); request->send(response); shouldReboot = true; });
	serverSecret.serveStatic(SF("/readEffect/").c_str(), SPIFFS, LedStripEffects::configFolder.c_str()).setCacheControl(SF("no-cache, no-store, must-revalidate").c_str());
	serverSecret.serveStatic(SF("/").c_str(), SPIFFS, SF("/ap/").c_str());
	serverSecret.addHandler(new SPIFFSEditor(SECRET_SERVER_USER, SECRET_SERVER_PASS));
	serverSecret.onNotFound([](AsyncWebServerRequest* request){
		if (request->url().startsWith(F("/css/")) || request->url().startsWith(F("/img/"))) {
			AsyncWebServerResponse* response = request->beginResponse(SPIFFS, SF("/www") + request->url());	//request->redirect(SF("http://") + WiFi.localIP().toString() + request->url());
			addNoCacheHeaders(response);
			request->send(response);
		} else {
			request->send(404, contentType_P[TYPE_PLAIN], String(F("Not found: ")) + request->url());
		}
	});
	serverSecret.begin();

	// Also, setup Arduino's OTA (only works from their IDE)
	#if USE_ARDUINO_OTA
		ArduinoOTA.setHostname(hostName);
		ArduinoOTA.setPort(ARDUINO_OTA_PORT);
		ArduinoOTA.setPassword(ARDUINO_OTA_PASS);
		ArduinoOTA.onStart([]() {
			consolePrintF("OTA: Start!\n");
			#if USE_OLED_DISP	
				display.clearDisplay();
				display.setCursor(0,0);
				display.println("OTA start!");
				display.display();
			#endif
			// Fade in and out the builtin led
			for(int i=PWMRANGE; i>0; i--) { analogWrite(LED_BUILTIN, i); delay(1); }
			for(int i=0; i<=PWMRANGE; i++) { analogWrite(LED_BUILTIN, i); delay(1); }
			pinMode(LED_BUILTIN, OUTPUT);	// ArduinoOTA blinks the LED_BUILTIN on progress, but digitalWrite doesn't work well after analogWrite. Solution is to reset the pin as output
		});
		ArduinoOTA.onEnd([]() {
			consolePrintF("OTA: Firmware update succeeded!\n");
			#if USE_OLED_DISP
				display.println("\n\nOTA ok! =)\nRestarting");
				display.display();
			#endif
			for (int i=0; i<30; i++) { analogWrite(LED_BUILTIN, (i*100) % 1001); delay(50); }
		});
		ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
			consolePrintF("OTA: %3u%% completed (%4u KB de %4u KB)\r", progress / (total/100), progress>>10, total>>10);
			#if USE_OLED_DISP
				static unsigned long next_display_refresh = millis();
				if (millis() > next_display_refresh) {
					next_display_refresh += 200;
					display.clearDisplay();
					display.setCursor(2,0);
					display.printf("OTA %3u%%\n   %4u KB\nde %4u KB", progress / (total/100), progress>>10, total>>10);
					display.display();
				}
			#endif
			// analogWrite(LED_BUILTIN, int((total-progress) / (total/PWMRANGE)));	// Recuerda que el builtin led es active low -> Luz apagada (0%) quiere decir escribir PWMRANGE; Luz encendida (100%) -> Escribir 0
		});
		ArduinoOTA.onError([](ota_error_t error) {
			consolePrintF("\nOTA: Error[%u]: ", error);
			#if USE_OLED_DISP
				display.printf("\n\nOTA error %u", error);
				display.display();
			#endif
			if (error == OTA_AUTH_ERROR) consolePrintF("Authentication failed\n");
			else if (error == OTA_BEGIN_ERROR) consolePrintF("Begin failed\n");
			else if (error == OTA_CONNECT_ERROR) consolePrintF("Connection failed\n");
			else if (error == OTA_RECEIVE_ERROR) consolePrintF("Reception failed\n");
			else if (error == OTA_END_ERROR) consolePrintF("End failed\n");
	
			consolePrintF("Rebooting Arduino...\n");
			ESP.restart();
		});
		ArduinoOTA.begin();
	#endif
	
	webSocketFFT.begin();
	webSocketFFT.onEvent(webSocketFFTevent);
	webSocketConsole.begin();
	webSocketConsole.onEvent(webSocketConsoleEvent);
}


/****************************************************/
/******      Web server related functions      ******/
/****************************************************/
void addNoCacheHeaders(AsyncWebServerResponse* response) {	// Add specific headers to an http response to avoid caching
	response->addHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
	response->addHeader(F("Pragma"), F("no-cache"));
	response->addHeader(F("Expires"), F("-1"));
}
/*bool isInterfaceAP(AsyncWebServer& server) {	// Returns true if client is connected through my own AP; false if connected on same WLAN
	return (server.client().localIP() == WiFi.softAPIP() || MAKE_SECRET_SETTS_PUBLIC);
}

bool ensureSecretSettingsVisibility(AsyncWebServer& server) {	// Makes sure secret settings can only be accessed from the AP network (not through WLAN). Returns true if client has permission to see the settings.
	if (!isInterfaceAP(server)) {
		server.send(403, contentType_P[TYPE_HTML], F("<html><head></head><body style='text-align: center;'><h1>Permission DENIED</h1><p>Sorry, you're not cool so you haven't been given permission to access the HotTub's secret settings.<br>Let the adults take care of the configuration. You just relax, that's what the hot tub is for!</p></body></html>"));
		return false;	// Get out of here, permission denied!
	}
	return true;		// All good! Show them the settings :)
}

void sendRedirect(String newUri, AsyncWebServer& server) {	// Send an HTTP 302 response so user is redirected to the appropriate web page
	server.sendHeader(F("Location"), newUri, true);	// Redirect them back to "/WiFi" so settings refresh
	server.send(302);	// 302 Found is often the code returned on redirections
}

void handleFileUpload(AsyncWebServer& server) {	// Allows user to upload a file to the SPIFFS (so we don't have to write the whole Flash via USB)
	static File fUpload;
	HTTPUpload& upload = server.upload();

	if (upload.status == UPLOAD_FILE_START){
		consolePrintF("\nUploading new SPIFFS file (to the temporary path %s) with upload.filename %s\n", UPLOAD_TEMP_FILENAME, upload.filename.c_str());
		fUpload = SPIFFS.open(UPLOAD_TEMP_FILENAME, "w");
	} else if (upload.status == UPLOAD_FILE_WRITE){
		if (fUpload)
			fUpload.write(upload.buf, upload.currentSize);
	} else if (upload.status == UPLOAD_FILE_END){
		if (fUpload)
			fUpload.close();
		consolePrintF("Successfully uploaded new SPIFFS file with upload.filename %s (%d B)!\n", upload.filename.c_str(), upload.totalSize);
	}
}

bool renameFileUpload(String fileName) {	// Renames the last file uploaded to the new file name provided
	if (!fileName.startsWith("/"))
		fileName = "/" + fileName;
	if (SPIFFS.exists(fileName)) {
		SPIFFS.remove(fileName);
		consolePrintF("\t(File already existed, removing old version -> Overwriting)\n");
	}
	
	bool r = SPIFFS.rename(UPLOAD_TEMP_FILENAME, fileName);
	consolePrintF("%s temporary file %s -> %s\n\n", (r? SF("Successfully renamed"):SF("Couldn't rename")).c_str(), UPLOAD_TEMP_FILENAME, fileName.c_str());

	return r;
}*/

/*void secretSettingsWLANedit(AsyncWebServerRequest* request) {	// Handles secret HTTP page that allows me to change WLAN settings
	if (!ensureSecretSettingsVisibility(serverSecret)) return;	// Make sure secret settings can only be accessed from the AP network (not through WLAN)

	sendNoCacheHeaders(serverSecret);
	serverSecret.setContentLength(CONTENT_LENGTH_UNKNOWN);
	serverSecret.send(200, CONT(TYPE_HTML));
	serverSecret.sendContent(F("<html><head><style>table{border-collapse: collapse;} th{text-align: right;} td{text-align: left;} th,td{padding: 3px 9px;}</style></head><body style='text-align: center;'><h1>WiFi secret config</h1><table align='center'>"));
	serverSecret.sendContent(SF("<tr><th>SoftAP config:</th><td>") + SOFT_AP_SSID + F(" (") + WiFi.softAPIP().toString() + F(")</td></tr>"));
	serverSecret.sendContent(SF("<tr><th>WLAN config:</th><td>") + WiFi.SSID() + F(" (") + WiFi.localIP().toString() + F(")</td></tr></table><br>"));

	// Now perform a scan and show all available networks so user can pick which one to connect to
	serverSecret.sendContent(F("<form method='POST' action='WiFiSave'><h3>Select WLAN network to connect to:</h3><table align='center'><tr><th>SSID:</th><td>"));
	int n = WiFi.scanNetworks();
	if (n > 0) {
		String opts = F("<select name='ssidDropdown' style='height: 18pt; width: 100%;'>");
		for (int i=0; i<n; ++i) {
			opts += SF("<option value='") + WiFi.SSID(i) + F("'") + ((WiFi.SSID(i)==String(wlanSSID))? F(" selected"):F("")) + F(">") + ((WiFi.encryptionType(i)==ENC_TYPE_NONE)? F("&nbsp;&#10004;&nbsp;&nbsp; "):F("&#128274; ")) + WiFi.SSID(i) + F(" [") + WiFi.RSSI(i) + F("dB]</option>");
		}
		opts += SF("</select><br> or <br><input type='checkbox' name='ssidManualChk' id='ssidManualChk'> <label for='ssidManualChk'>Manual SSID:</label> ");
		serverSecret.sendContent(opts);
	} else {
		serverSecret.sendContent(F("No WLAN networks found, gonna have to enter SSID manually:<br><input type='hidden' name='ssidManualChk' value='on'/>"));
	}
	serverSecret.sendContent(SF("<input type='text' placeholder='Network SSID' name='ssidManualTxt' value='") + String(wlanSSID) + F("'/></td></tr>"
		"<tr><th>Password:</th><td><input type='password' placeholder='password' name='pass' value='") + String(wlanPass) + F("'/></td></tr>"
		"<tr><th>Static IP:</th><td><input type='text' placeholder='Static IP' name='ip' value='") + wlanMyIP.toString() + F("'/></td></tr>"
		"<tr><th>Gateway:</th><td><input type='text' placeholder='Gateway' name='gateway' value='") + wlanGateway.toString() + F("'/></td></tr>"
		"<tr><th>Net mask:</th><td><input type='text' placeholder='Net mask' name='mask' value='") + wlanMask.toString() + F("'/></td></tr></table>"
		"<p><input type='submit' value='Connect/Disconnect'/></p></form></body></html>"));
}*/

void secretSettingsWLANscan(AsyncWebServerRequest* request) {	// Handles secret HTTP page that scans WLAN networks
	String json = SF("{\"currAP\":{\"ssid\":\"") + SOFT_AP_SSID + F("\",\"ip\":\"") + WiFi.softAPIP().toString() + F("\"},"
		"\"currWLAN\":{\"ssid\":\"") + String(wlanSSID) + F("\",\"pass\":\"") + String(wlanPass) + F("\",\"ip\":\"") + wlanMyIP.toString() + F("\",\"gateway\":\"") + wlanGateway.toString() + F("\",\"mask\":\"") + wlanMask.toString() + F("\"},"
		"\"nets\":[");
	int n = WiFi.scanComplete();
	
	if (n == -2) {
		WiFi.scanNetworks(true);
	} else {
		for (int i=0; i<n; ++i) {
			if (i) json += ",";
			json += SF("{"
				"\"rssi\":") + String(WiFi.RSSI(i)) + F(","
				"\"ssid\":\"") + WiFi.SSID(i) + F("\","
				"\"bssid\":\"") + WiFi.BSSIDstr(i) + F("\","
				"\"channel\":") + String(WiFi.channel(i)) + F(","
				"\"secure\":") + String(WiFi.encryptionType(i)) + F(","
				"\"hidden\":") + String(WiFi.isHidden(i)) + F(
			"}");
		}
		WiFi.scanDelete();
		if(WiFi.scanComplete() == -2){
			WiFi.scanNetworks(true);
		}
	}
	
	json += "]}";
	AsyncWebServerResponse* response = request->beginResponse(200, CONT(TYPE_JSON), json);
	addNoCacheHeaders(response);
	request->send(response);
	json = String();
}

void secretSettingsWLANsave(AsyncWebServerRequest* request) {	// Handles secret HTTP page that saves new WLAN settings
	/*if (!ensureSecretSettingsVisibility(serverSecret)) return;	// Make sure secret settings can only be accessed from the AP network (not through WLAN)
	if (serverSecret.method() != HTTP_POST)
		return serverSecret.send(404, CONT(TYPE_PLAIN), SF("Not found: ") + serverSecret.uri());*/

	consolePrintF("Received request to save new WLAN settings!\n");
	bool bSSIDmanual = false;
	if (request->hasArg(CF("ssidManualChk"))) bSSIDmanual = (request->arg(F("ssidManualChk"))=="on");
	if (request->hasArg(bSSIDmanual? CF("ssidManualTxt"):CF("ssidDropdown"))) request->arg(bSSIDmanual? F("ssidManualTxt"):F("ssidDropdown")).toCharArray(wlanSSID, sizeof(wlanSSID)-1);
	if (request->hasArg(CF("pass"))) request->arg("pass").toCharArray(wlanPass, sizeof(wlanPass)-1);
	if (request->hasArg(CF("ip"))) wlanMyIP.fromString(request->arg(F("ip")));
	if (request->hasArg(CF("gateway"))) wlanGateway.fromString(request->arg(F("gateway")));
	if (request->hasArg(CF("mask"))) wlanMask.fromString(request->arg(F("mask")));
	saveWLANconfig();	// Write new settings to EEPROM

	AsyncWebServerResponse* response = request->beginResponse(302);
	response->addHeader(F("Location"), F("WiFi"));	// Redirect them back to "/WiFi" so settings refresh
	addNoCacheHeaders(response);
	request->send(response);

	if (strlen(wlanSSID) > 0) connectToWLAN();	// Connect/Reconnect WLAN with new credentials if there is an SSID
}

void secretSettingsListLEDeffects(AsyncWebServerRequest* request) {	// Lists all config files related to LED strip effects
//	if (!ensureSecretSettingsVisibility(serverSecret)) return;	// Make sure secret settings can only be accessed from the AP network (not through WLAN)

	String s = SF("<html><head><style>table{border-collapse: collapse;} th{font-weight: bold;} th,td{padding: 3px 9px; text-align: center;} a:link,a:visited{text-decoration: none; color: #22c;} a:hover,a:active{text-decoration:underline; color: #33f;}</style></head>"
		"<body style='text-align: center;'><h1>LED strip effects secret config</h1><table align='center'><tr><th>File name</th><th>File size</th></tr>");
	
	Dir dir = SPIFFS.openDir(LedStripEffects::configFolder);
	while (dir.next()) {
		File file = dir.openFile("r");
		String fileName = String(file.name()).substring(String(file.name()).lastIndexOf("/") + 1);	// Remove parent directories from file name
		s += SF("<tr><th><a href='readEffect/") + fileName + F("' target='iFileContents'>") + fileName + F("</a></th><td>") + file.size() + F("B</td>");
		file.close();
	}
	
	s += SF("</table><br><iframe style='width: 500px; height: 200px; overflow: auto; border: 1px solid black; display: block; margin: auto;' name='iFileContents' title='See the contents of the files here :)' /></html>");
	AsyncWebServerResponse* response = request->beginResponse(200, CONT(TYPE_HTML), s);
	addNoCacheHeaders(response);
	request->send(response);
	s = String();
}

void webSocketFFTevent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {	// webSocketFFT event callback function
	IPAddress ip;
	switch(type) {
	case WStype_ERROR:
		consolePrintF("[WebSocket %u] Error: %s\n", num, payload);
		break;
	case WStype_DISCONNECTED:
		consolePrintF("[WebSocket %u] Disconnected!\n", num);
		break;
	case WStype_CONNECTED:
		ip = webSocketFFT.remoteIP(num);
		consolePrintF("[WebSocket %u] Connected from %d.%d.%d.%d, URL %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
		webSocketFFT.broadcastTXT("Connected");	// send message to client to confirm connection ok
		break;
	case WStype_TEXT:
		consolePrintF("[WebSocket %u] Rx text message: %s\n", num, payload);
		break;
	case WStype_BIN:
		consolePrintF("[WebSocket %u] Rx binary message:\n", num);
		hexdump(payload, lenght);
		break;
	}
}

void webSocketConsoleEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {	// webSocketConsole event callback function
	switch(type) {
	case WStype_CONNECTED:
		webSocketConsole.broadcastTXT("Connected");	// send message to client to confirm connection ok
		break;
	case WStype_ERROR:
	case WStype_DISCONNECTED:
	case WStype_TEXT:
	case WStype_BIN:
	default:
		break;
	}
}

void consolePrintf(const char * format, ...) {	// Log messages through webSocketConsole and Serial
	char buf[1024];
	va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
	webSocketConsole.broadcastTXT(buf);
	Serial.printf(buf);
}

int constexpr precompute_strlen(const char* str) {
    return *str ? 1 + precompute_strlen(str + 1) : 0;
}

void processWebServer() {	// "secretSettings.loop()" function: handle incoming OTA connections (if any), secret settings http requests and webSocket events
	static uint32_t last_t_sec = 0;
	uint32_t t_msec = curr_time%1000, t_sec = curr_time/1000, t_min = t_sec/60, t_hr = t_min/60; t_sec %= 60; t_min %= 60;
//	t_sec = (curr_time>>5) & 0x3;	// Every 32ms
	if (t_sec != last_t_sec) {
		last_t_sec = t_sec;
		consolePrintF("Still alive (t=%3d:%02d'%02d\"); cur vol: %10d, avg vol: %10d; HEAP: %5d B\n", t_hr, t_min, t_sec, int(curr_volume), int(avg_volume), ESP.getFreeHeap());
	}

	if (adc_buf_got_full) {
		adc_buf_got_full = false;	// Remember to reset this flag so we only send when the next buffer is full ;)
		unsigned int buf_id = !adc_buf_id_current;	// Use the *opposite* buffer id of the one being filled currently (so we send the one that's already full)

		performFFT(buf_id);

		webSocketFFT.broadcastBIN(reinterpret_cast<uint8_t*>(fft_real), sizeof(fft_real[0])*(1 + N_FFT/2));
	}
	webSocketFFT.loop();
	webSocketConsole.loop();

	if (shouldReboot) ESP.restart();	// AsyncWebServer doesn't suggest rebooting from async callbacks, so we set a flag and reboot from here :)
	
	#if USE_ARDUINO_OTA
		ArduinoOTA.handle();
	#endif
}

