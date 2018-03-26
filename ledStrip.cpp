/******      LED strip      ******/
#include "ledStrip.h"

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(N_PIXELS, LED_PIN);
LedStripEffects stripEffects;


/***************************************************/
/******            SETUP FUNCTIONS            ******/
/***************************************************/
void setupLedStrip() {	// Initializes LED strip and the effect list stripEffects
	stripEffects.loadConfigFromFile();

	stripEffects.clear();
	stripEffects.addEffect(new EffectVolumeShifter(15000));
	stripEffects.addEffect(new EffectColorWipe(RgbColor(255,0,0), 30));
	stripEffects.addEffect(new EffectRainbow());
	stripEffects.addEffect(new EffectColorWipe(RgbColor(0,255,0), 20));
	stripEffects.addEffect(new EffectRainbowCycle());
	stripEffects.addEffect(new EffectColorWipe(RgbColor(0,0,255), 10));
	stripEffects.addEffect(new EffectTheaterChaseRainbow(3));
	stripEffects.saveConfigToFile();
	stripEffects.restartEffectList();
	
	strip.Begin();
	strip.Show();
}


/***************************************************/
/******      LED strip related functions      ******/
/***************************************************/
uint32_t rgbColorToInt(RgbColor c) {	// Converts an RgbColor to uint32_t so JSON can parse it
	return ((c.B<<16) | (c.G<<8) | (c.R));
}

RgbColor intToRgbColor(uint32_t c) {	// Converts a uint32_t back to RgbColor
	return RgbColor(c&0xFF, (c>>8)&0xFF, (c>>16)&0xFF);
}

void colorFull(RgbColor c) {	// Fills the whole strip with given color
	for(uint16_t i=0; i<strip.PixelCount(); i++) {
		strip.SetPixelColor(i, c);
	}
	strip.Show();
}

RgbColor Wheel(byte WheelPos) {	// Sort of HSV color generation (WheelPos is an approx. of a 0-255 hue value)
	WheelPos = 255 - WheelPos;
	if(WheelPos < 85) {
		return RgbColor(255 - WheelPos * 3, 0, WheelPos * 3);
	}
	if(WheelPos < 170) {
		WheelPos -= 85;
		return RgbColor(0, WheelPos * 3, 255 - WheelPos * 3);
	}
	WheelPos -= 170;
	return RgbColor(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void processLedStrip() {	// "LEDstrip.loop()" function: executes an iteration of the current effect
	stripEffects.loop();
}


/***************************************************/
/******           LED strip effects           ******/
/***************************************************/
/**********************      LedStripEffect      **********************/
const char PROGMEM LedStripEffect::strCompressedEffectName[] = {""};
const char PROGMEM LedStripEffect::strReadableEffectName  [] = {""};
const char PROGMEM LedStripEffect::strReadableEffectDesc  [] = {""};

LedStripEffect* LedStripEffect::fromEffectName(String effectName) {	// (Dynamically) creates a new LedStripEffect of the right derived class based on effectName and passes configPath to its constructor
	if        (effectName == FPSTR(EffectColorWipe::strCompressedEffectName)) {
		return new EffectColorWipe();
	} else if (effectName == FPSTR(EffectRainbow::strCompressedEffectName)) {
		return new EffectRainbow();
	} else if (effectName == FPSTR(EffectRainbowCycle::strCompressedEffectName)) {
		return new EffectRainbowCycle();
	} else if (effectName == FPSTR(EffectTheaterChase::strCompressedEffectName)) {
		return new EffectTheaterChase();
	} else if (effectName == FPSTR(EffectTheaterChaseRainbow::strCompressedEffectName)) {
		return new EffectTheaterChaseRainbow();
	} else if (effectName == FPSTR(EffectVolumeShifter::strCompressedEffectName)) {
		return new EffectVolumeShifter();
	}

	return NULL;
}

LedStripEffect* LedStripEffect::fromJson(String configPath) {	// (Dynamically) creates and configures a new LedStripEffect of the right derived class based on the contents of the supplied JSON configuration file
	std::unique_ptr<char[]> buf = readFile(configPath);
	if (!buf) return NULL;	// Only parse the JSON if readFile succeeded (it will print errors to the console if not)
	
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(buf.get());
	if (!json.success()) {
		consolePrintF("Failed to parse JSON config file %s :(\n", configPath.c_str());
		return NULL;
	}

	LedStripEffect* effect = LedStripEffect::fromEffectName(json["effectName"]);
	if (effect) effect->loadConfigFromJson(json);	// (pure virtual function) Load the effect settings based on the specific effect class implementation
//	if (effect) consolePrintF("Loaded %s" + effect->toString() + " from " + configPath + "\n");
	return effect;
}

void LedStripEffect::preEffectReset(bool resetCntLoops) {	// Resets all effect related variables before executing the first iteration of the effect
	strip.ClearTo(RgbColor(0));	// Clear screen (some effects assume the strip to be off before starting)
	tNextIteration = curr_time;	// Initialize tNextIteration to guarantee at least one iteration of the effect right now
	resetCounters();			// Reset any effect-specific counters to make sure it starts from the beginning
	if (resetCntLoops)
		cntLoops = 0;
	consolePrintF("About to start iteration %d/%d of %s\n", cntLoops+1, numLoops, toString().c_str());
}

bool LedStripEffect::loop() {	// Runs as many iterations of the effect as needed based on current time and then returns whether the effect is done (true) or not (false)
	while (curr_time >= tNextIteration) {	// If tickInterval is too small (compared to how often loop() gets called), might be necessary to perform more than one iteration -> "while" instead of "if"
		if (tickInterval == (uint16_t)-1) {	// tickInterval=-1 is a special case, it indicates we only want the effect to be executed once per LedStripEffect::loop call (instead of every tickInterval ms)
			tNextIteration = curr_time + 1;	// so set tNextIteration to curr_time+1, to make sure we exit the while loop after one execution of the effect
		} else {
			tNextIteration += tickInterval;	// Otherwise, for a regular iterval'd effect, compute the "due date" for the next iteration
		}
		didOneIter = false;					// Initialize didOneIter to false (effectFunc will use this aux var to return right after performing one effect iteration)
		
		if (effectFunc()) {					// Perform one iteration of the effect
			consolePrintF("%s finished iteration %d/%d!\n", getReadableEffectName().c_str(), cntLoops+1, numLoops);
			cntLoops++;						// Increase the "full effect" counter
			if (cntLoops >= numLoops) {		// Check if we've completed the desired number of "full iterations" of the effect
				return true;				// (Only) in case that was the last iteration of the last loop of the effect, return true
			}
			preEffectReset(false);			// Otherwise, restart the effect: reset all vars/counters EXCEPT for cntLoops (that's what the 'false' is for)
		}
	}
	
	return false;							// Effect not done yet, but nothing else to execute for now
}


/**********************      LedStripEffects      **********************/
const String LedStripEffects::configFolder("/ledEffects");					// Indicates the path to the folder where all the effect-related config files is stored in the SPIFFS
const String LedStripEffects::configFile(configFolder + "/numEffects.json");// Indicates the path to the SPIFFS file where we store how many effects we stored in 'configFolder'
const String LedStripEffects::configFilePrefix(configFolder + "/effect");	// Indicates the prefix for the SPIFFS files containing config settings for each individual effect

void LedStripEffects::loadDefaultEffectList() {	// Loads the default effect list (useful for example if loading the config from file failed)
	clear();
	addEffect(new EffectVolumeShifter(30000));
}

bool LedStripEffects::loadConfigFromFile(String configPath) {	// Loads effect list from SPIFFS file located at configPath
	consolePrintF("\nTrying to read LED strip effect list config from %s...\n", configPath.c_str());
	std::unique_ptr<char[]> buf = readFile(configPath);
	if (!buf) {	// Only parse the JSON if readFile succeeded (it will print errors to the console if not)
		loadDefaultEffectList();
		return false;
	}
	
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(buf.get());
	if (!json.success()) {
		consolePrintF("Failed to parse JSON config file %s :(\n\n", configPath.c_str());
		loadDefaultEffectList();
		return false;
	}

	clear();
	for (uint8_t i=0; i<json["numEffects"]; ++i) {
		addEffect(LedStripEffect::fromJson(LedStripEffects::configFilePrefix + String(i) + ".json"));
	}
	consolePrintF("Successfully loaded LED strip effect list config from %s!\n\n", configPath.c_str());
	return true;
}

bool LedStripEffects::saveConfigToFile(String configPath) {	// Stores current list of effects and their configuration to a SPIFFS JSON file (so settings can be loaded on reboot)
	// First, delete old config files to avoid issues
	Dir dir = SPIFFS.openDir(LedStripEffects::configFolder);
	consolePrintF("\nBefore saving effect list config, I'm going to delete old config files:\n");
	while (dir.next()) {
		consolePrintF("\t%s... ", dir.fileName().c_str());
		if (SPIFFS.remove(dir.fileName())) {
			consolePrintF("success!\n");
		} else {
			consolePrintF("fail!\n");
		}
	}

	// Then, actually save JSON files with current effect list config
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();

	json["numEffects"] = listEffects.size();

	if (!saveJSON(json, configPath)) {
		consolePrintF("Couldn't save LED strip effect list! :(\n\n");
		return false;
	}

	for (size_t i=0; i<listEffects.size(); ++i) {
		listEffects[i]->saveConfigToFile(LedStripEffects::configFilePrefix + String(i) + ".json");
//		consolePrintF("Saved %s\n", listEffects[i]->toString());
	}
	consolePrintF("Successfully saved LED strip effect list config to %s!\n\n", configPath.c_str());
	return true;
}

void LedStripEffects::restartEffectList() {	// Restarts the effect list: goes back to the first iteration of the first effect
	currEffect = -1;
	nextEffect();
}

void LedStripEffects::nextEffect() {
	if (listEffects.empty()) {
		currEffect = 0;
	} else {	// ***can't "X mod 0" or it'll crash***, so that's why we take different action if list is empty
		currEffect = (currEffect+1) % listEffects.size();	// Increase the effect counter
		listEffects[currEffect]->preEffectReset();			// And reset any effect related variables/counters
	}
}

void LedStripEffects::addEffect(LedStripEffect* effect) {
	if (effect) listEffects.push_back(effect);	// Don't add effect if it's NULL ;)
}

void LedStripEffects::removeEffect(uint8_t n, bool restart) {
	if (n < listEffects.size()) {
		delete listEffects[n];
		listEffects.erase(listEffects.begin() + n);
		if (restart) restartEffectList();
	}
}

void LedStripEffects::clear() {
	while (!listEffects.empty()) {
		removeEffect(0, false);
	}
	currEffect = 0;
}

void LedStripEffects::loop() {
	if (!listEffects.empty()) {
		if (listEffects[currEffect]->loop()) {
			nextEffect();
		}
	}
}


/**********************      EffectColorWipe      **********************/
const char PROGMEM EffectColorWipe::strCompressedEffectName[] = {"ClrWipe"};
const char PROGMEM EffectColorWipe::strReadableEffectName  [] = {"Color wipe"};
const char PROGMEM EffectColorWipe::strReadableEffectDesc  [] = {"Fills the strip with a specific color one led at a time from start to end"};

const RgbColor EffectColorWipe::defaultColor = RgbColor(255, 0, 0);
const uint16_t EffectColorWipe::defaultTickInterval = 20;
const uint8_t  EffectColorWipe::defaultNumLoops = 1;

bool EffectColorWipe::saveConfigToFile(String configPath) {	// Stores current values of all variables related to the effect (so settings can be loaded on reboot)
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();

	json["effectName"] = getCompressedEffectName();
	json["color"] = rgbColorToInt(color);
	json["tickInterval"] = tickInterval;
	json["numLoops"] = numLoops;

	return saveJSON(json, configPath);
}

bool EffectColorWipe::loadConfigFromJson(JsonObject& json) {	// Loads effect settings from JSON buffer (already parsed)
	color = intToRgbColor(json["color"]);
	tickInterval = json["tickInterval"];
	numLoops = json["numLoops"];

	return true;
}

bool EffectColorWipe::effectFunc() {
	for (; i<N_PIXELS; ++i) {
		if (didOneIter) return false;   // Already completed one iteration, return false (=not finished yet)

		strip.SetPixelColor(i, color);
		strip.Show();
		didOneIter = true;
	}

	return true;
}


/**********************      EffectRainbow      **********************/
const char PROGMEM EffectRainbow::strCompressedEffectName[] = {"Rbow"};
const char PROGMEM EffectRainbow::strReadableEffectName  [] = {"Rainbow"};
const char PROGMEM EffectRainbow::strReadableEffectDesc  [] = {"Fills the entire strip with the colors of the rainbow and slowly rotates them around"};

const uint16_t EffectRainbow::defaultTickInterval = 20;
const uint8_t  EffectRainbow::defaultNumLoops = 1;

bool EffectRainbow::saveConfigToFile(String configPath) {	// Stores current values of all variables related to the effect (so settings can be loaded on reboot)
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();

	json["effectName"] = getCompressedEffectName();
	json["tickInterval"] = tickInterval;
	json["numLoops"] = numLoops;

	return saveJSON(json, configPath);
}

bool EffectRainbow::loadConfigFromJson(JsonObject& json) {	// Loads effect settings from JSON buffer (already parsed)
	tickInterval = json["tickInterval"];
	numLoops = json["numLoops"];

	return true;
}

bool EffectRainbow::effectFunc() {
	for(; i<256; ++i) {
		if (didOneIter) return false;   // Already completed one iteration, return false (=not finished yet)

		for(uint16_t j=0; j<N_PIXELS; ++j) {
			strip.SetPixelColor(j, Wheel((i+j) & 255));
		}
		strip.Show();
		didOneIter = true;
	}

	return true;
}


/**********************      EffectRainbowCycle      **********************/
const char PROGMEM EffectRainbowCycle::strCompressedEffectName[] = {"RbowCyc"};
const char PROGMEM EffectRainbowCycle::strReadableEffectName  [] = {"Rainbow cycle"};
const char PROGMEM EffectRainbowCycle::strReadableEffectDesc  [] = {"Fills the entire strip with the colors of the rainbow and slowly rotates them around"};

const uint16_t EffectRainbowCycle::defaultTickInterval = 20;
const uint8_t  EffectRainbowCycle::defaultNumLoops = 3;

bool EffectRainbowCycle::saveConfigToFile(String configPath) {	// Stores current values of all variables related to the effect (so settings can be loaded on reboot)
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();

	json["effectName"] = getCompressedEffectName();
	json["tickInterval"] = tickInterval;
	json["numLoops"] = numLoops;

	return saveJSON(json, configPath);
}

bool EffectRainbowCycle::loadConfigFromJson(JsonObject& json) {	// Loads effect settings from JSON buffer (already parsed)
	tickInterval = json["tickInterval"];
	numLoops = json["numLoops"];

	return true;
}

bool EffectRainbowCycle::effectFunc() {
	for(; i<256; i++) {
		if (didOneIter) return false;   // Already completed one iteration, return false (=not finished yet)

		for(uint16_t j=0; j<N_PIXELS; ++j) {
			strip.SetPixelColor(j, Wheel(((j * 256 / N_PIXELS) + i) & 255));
		}
		strip.Show();
		didOneIter = true;
	}

	return true;
}


/**********************      EffectTheaterChase      **********************/
const char PROGMEM EffectTheaterChase::strCompressedEffectName[] = {"Chase"};
const char PROGMEM EffectTheaterChase::strReadableEffectName  [] = {"Theater chase"};
const char PROGMEM EffectTheaterChase::strReadableEffectDesc  [] = {"Creates a set of 'particles' of a specific color which rotate around chasing each other"};

const RgbColor EffectTheaterChase::defaultColor = RgbColor(255, 0, 0);
const uint8_t  EffectTheaterChase::defaultStep = 5;
const uint16_t EffectTheaterChase::defaultTickInterval = 50;
const uint8_t  EffectTheaterChase::defaultNumLoops = 5;

bool EffectTheaterChase::saveConfigToFile(String configPath) {	// Stores current values of all variables related to the effect (so settings can be loaded on reboot)
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();

	json["effectName"] = getCompressedEffectName();
	json["color"] = rgbColorToInt(color);
	json["step"] = step;
	json["tickInterval"] = tickInterval;
	json["numLoops"] = numLoops;

	return saveJSON(json, configPath);
}

bool EffectTheaterChase::loadConfigFromJson(JsonObject& json) {	// Loads effect settings from JSON buffer (already parsed)
	color = intToRgbColor(json["color"]);
	step = json["step"];
	tickInterval = json["tickInterval"];
	numLoops = json["numLoops"];

	return true;
}

bool EffectTheaterChase::effectFunc() {
	for (; i<step; ++i) {
		if (didOneIter) return false;   // Already completed one iteration, return false (=not finished yet)

		for (uint16_t j=0; j<N_PIXELS; j+=step) {
			strip.SetPixelColor(i+j, color);	// Turn on every other 'step' pixel
		}
		strip.Show();
		didOneIter = true;

		for (uint16_t j=0; j<N_PIXELS; j+=step) {
			strip.SetPixelColor(i+j, 0);	// Turn off every other 'step' pixel
		}
	}

	return true;
}


/**********************      EffectTheaterChaseRainbow      **********************/
const char PROGMEM EffectTheaterChaseRainbow::strCompressedEffectName[] = {"ChaseRbow"};
const char PROGMEM EffectTheaterChaseRainbow::strReadableEffectName  [] = {"Rainbow theater chase"};
const char PROGMEM EffectTheaterChaseRainbow::strReadableEffectDesc  [] = {"Creates a set of 'particles' of the colors of the rainbow which rotate around chasing each other"};

const uint8_t  EffectTheaterChaseRainbow::defaultStep = 5;
const uint16_t EffectTheaterChaseRainbow::defaultTickInterval = 50;
const uint8_t  EffectTheaterChaseRainbow::defaultNumLoops = 1;

bool EffectTheaterChaseRainbow::saveConfigToFile(String configPath) {	// Stores current values of all variables related to the effect (so settings can be loaded on reboot)
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();

	json["effectName"] = getCompressedEffectName();
	json["step"] = step;
	json["tickInterval"] = tickInterval;
	json["numLoops"] = numLoops;

	return saveJSON(json, configPath);
}

bool EffectTheaterChaseRainbow::loadConfigFromJson(JsonObject& json) {	// Loads effect settings from JSON buffer (already parsed)
	step = json["step"];
	tickInterval = json["tickInterval"];
	numLoops = json["numLoops"];

	return true;
}

bool EffectTheaterChaseRainbow::effectFunc() {
	for (; i<256; ++i) {     // cycle all 256 colors in the wheel
		for (; j<step; ++j) {
			if (didOneIter) return false;   // Already completed one iteration, return false (=not finished yet)

			for (uint16_t k=0; k<N_PIXELS; k+=step) {
				strip.SetPixelColor(k+j, Wheel((k+i) % 255));	// Turn on every other 'step' pixel
			}
			strip.Show();
			didOneIter = true;

			for (uint16_t k=0; k<N_PIXELS; k+=step) {
				strip.SetPixelColor(k+j, 0);	// Turn off every other 'step' pixel
			}
		}
		j = 0;
	}

	return true;
}


/**********************      EffectVolumeShifter      **********************/
const char PROGMEM EffectVolumeShifter::strCompressedEffectName[] = {"VolShift"};
const char PROGMEM EffectVolumeShifter::strReadableEffectName  [] = {"Volume shifter"};
const char PROGMEM EffectVolumeShifter::strReadableEffectDesc  [] = {"Lights follow the volume of the music by changing color and intensity"};

const uint32_t EffectVolumeShifter::defaultTeffectLength = 30000;
const uint16_t EffectVolumeShifter::defaultTickInterval = -1;
const uint8_t  EffectVolumeShifter::defaultNumLoops = 1;

bool EffectVolumeShifter::saveConfigToFile(String configPath) {	// Stores current values of all variables related to the effect (so settings can be loaded on reboot)
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();

	json["effectName"] = getCompressedEffectName();
	json["tEffectLength"] = tEffectLength;
	json["tickInterval"] = tickInterval;
	json["numLoops"] = numLoops;

	return saveJSON(json, configPath);
}

bool EffectVolumeShifter::loadConfigFromJson(JsonObject& json) {	// Loads effect settings from JSON buffer (already parsed)
	tEffectLength = json["tEffectLength"];
	tickInterval = json["tickInterval"];
	numLoops = json["numLoops"];

	return true;
}

bool EffectVolumeShifter::effectFunc() {
	if (curr_time>=tDeadlineEffect && tEffectLength!=(uint32_t)-1)
		return true;

	strip.ShiftRight(1);
	strip.SetPixelColor(0, HsbColor(0.6+0.4*curr_volume/(2*avg_volume), 1, (curr_volume > 1.5*avg_volume)?1:0.2));
	strip.Show();
	return false;
}

