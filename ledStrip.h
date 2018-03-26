/******      LED strip      ******/
#ifndef LED_STRIP_H_
#define LED_STRIP_H_

#include "main.h"						// HotTub global includes and definitions
#include "FFT.h"						// FFT library so we can make effects that depend on current sound
#include "fileIO.h"						// File IO library contains SPIFFS filesystem and JSON parsers
#include <NeoPixelBus.h>				// LED strip
#include <vector>

#define N_PIXELS		450
#define LED_PIN			3

extern NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip;
class LedStripEffect;
class LedStripEffects;
extern LedStripEffects stripEffects;


/***************************************************/
/******            SETUP FUNCTIONS            ******/
/***************************************************/
void setupLedStrip();	// Initializes LED strip and the effect list stripEffects


/***************************************************/
/******      LED strip related functions      ******/
/***************************************************/
uint32_t rgbColorToInt(RgbColor c);	// Converts an RgbColor to uint32_t so JSON can parse it
RgbColor intToRgbColor(uint32_t c);	// Converts a uint32_t back to RgbColor
void colorFull(RgbColor c);	// Fills the whole strip with given color
RgbColor Wheel(byte WheelPos);	// Sort of HSV color generation (WheelPos is an approx. of a 0-255 hue value)
void processLedStrip();	// "LEDstrip.loop()" function: executes an iteration of the current effect


/***************************************************/
/******           LED strip effects           ******/
/***************************************************/
/**********************      LedStripEffect      **********************/
class LedStripEffect {	// Abstract class defining a general led strip effect (eg, turn all leds on to a specific color, rainbow effect, follow the music...)
public:
	LedStripEffect(uint16_t tickInterval=25, uint8_t numLoops=1) : tickInterval(tickInterval), numLoops((numLoops>254)? 254:numLoops) {}
	
	uint16_t tickInterval;	// "speed": interval in ms between iterations
	uint8_t numLoops;		// Number of times to run the whole effect (in case the same effect wants to be played multiple times in a row). Defaults to 1

	static const char PROGMEM strCompressedEffectName[], strReadableEffectName[], strReadableEffectDesc[];	// Static constants to hold the unique string that getCompressedEffectName, etc. return
	virtual String getCompressedEffectName() = 0;			// Returns "compressed" name of the effect (for saving/loading settings files)
	virtual String getReadableEffectName() = 0;				// Returns human-readable name of the effect (to show in public settings)
	virtual String getReadableEffectDesc() = 0;				// Returns human-readable description of the effect (to show in public settings)
	virtual String toString() = 0;							// Returns a String description of the state of the effect (name, var values...) { return getReadableEffectName() + " (tick:" + tickInterval + "ms; loops:" + numLoops + ")"; }

	static LedStripEffect* fromEffectName(String effectName);//(Dynamically) creates a new LedStripEffect of the right derived class based on effectName
	static LedStripEffect* fromJson(String configPath);		// (Dynamically) creates and configures a new LedStripEffect of the right derived class based on the contents of the supplied JSON configuration file
	virtual bool loadConfigFromJson(JsonObject& json) = 0;	// Loads effect settings from JSON buffer (already parsed)
	virtual bool saveConfigToFile(String configPath) = 0;	// Stores current values of all variables related to the effect (so settings can be loaded on reboot)
	
	virtual bool effectFunc() = 0;							// Controls the led strip in whichever way the effect wants. Returns false at the end of every iteration but the last one (true)
	virtual void resetCounters() = 0;						// Resets any counters/variables so that the effect starts back at the first iteration
	
	void preEffectReset(bool resetCntLoops=true);			// Resets all effect related variables before executing the first iteration of the effect
	bool loop();	// Runs as many iterations of the effect as needed based on current time and then returns whether the effect is done (true) or not (false)

protected:
	uint32_t tNextIteration;// Time (ms) after which a new iteration of the effect will be exectued
	bool didOneIter;		// Auxiliary variable that effectFunc() will use to exit right after performing one iteration
	uint8_t cntLoops;		// Counter to keep track of how many times the whole effect has been executed in a row
};

/**********************      LedStripEffects      **********************/
class LedStripEffects {
public:
	LedStripEffects() : currEffect(0) {}

	static const String configFolder;		// Indicates the path to the folder where all the effect-related config files is stored in the SPIFFS
	static const String configFile;			// Indicates the path to the SPIFFS file where we store how many effects we stored in 'configFolder'
	static const String configFilePrefix;	// Indicates the prefix for the SPIFFS files containing config settings for each individual effect

	void loadDefaultEffectList();							// Loads the default effect list (useful for example if loading the config from file failed)
	bool loadConfigFromFile(String configPath=configFile);	// Loads effect list from SPIFFS JSON file located at configPath
	bool saveConfigToFile(String configPath=configFile);	// Stores current list of effects and their configuration to a SPIFFS JSON file (so settings can be loaded on reboot)
	void restartEffectList();
	void nextEffect();
	void addEffect(LedStripEffect* effect);
	void removeEffect(uint8_t n, bool restart=true);
	void clear();
	void loop();

protected:
	std::vector<LedStripEffect*> listEffects;
	uint8_t currEffect;	// Counter to keep track of how many times the whole effect has been executed in a row (useful if we want to repeat the same effect multiple times)
};

/**********************      EffectColorWipe      **********************/
class EffectColorWipe : public LedStripEffect {
public:
	EffectColorWipe(RgbColor color=defaultColor, uint16_t tickInterval=defaultTickInterval, uint8_t numLoops=defaultNumLoops) : LedStripEffect(tickInterval, numLoops), color(color) {}
//	EffectColorWipe(String configPath) : EffectColorWipe() { loadConfigFromFile(configPath); }

	RgbColor color;
	static const RgbColor defaultColor;
	static const uint16_t defaultTickInterval;
	static const uint8_t  defaultNumLoops;

	static const char PROGMEM strCompressedEffectName[], strReadableEffectName[], strReadableEffectDesc[];	// Static constants to hold the unique string that getCompressedEffectName, etc. return
	String getCompressedEffectName() { return FPSTR(EffectColorWipe::strCompressedEffectName); }
	String getReadableEffectName()   { return FPSTR(EffectColorWipe::strReadableEffectName); }
	String getReadableEffectDesc()   { return FPSTR(EffectColorWipe::strReadableEffectDesc); }
	String toString() { return getReadableEffectName() + " (c:[" + color.R + "," + color.G + "," + color.B + "]; tick:" + tickInterval + "ms; loops:" + numLoops + ")"; }

	bool saveConfigToFile(String configPath);	// Stores current values of all variables related to the effect (so settings can be loaded on reboot)
	bool loadConfigFromJson(JsonObject& json);	// Loads effect settings from JSON buffer (already parsed)
	
	void resetCounters() { i = 0; }
	bool effectFunc();

protected:
	uint16_t i;
};

/**********************      EffectRainbow      **********************/
class EffectRainbow : public LedStripEffect {
public:
	EffectRainbow(uint16_t tickInterval=defaultTickInterval, uint8_t numLoops=defaultNumLoops) : LedStripEffect(tickInterval, numLoops) {}
//	EffectRainbow(String configPath) : EffectRainbow() { loadConfigFromFile(configPath); }

	static const uint16_t defaultTickInterval;
	static const uint8_t  defaultNumLoops;

	static const char PROGMEM strCompressedEffectName[], strReadableEffectName[], strReadableEffectDesc[];	// Static constants to hold the unique string that getCompressedEffectName, etc. return
	String getCompressedEffectName() { return FPSTR(EffectRainbow::strCompressedEffectName); }
	String getReadableEffectName()   { return FPSTR(EffectRainbow::strReadableEffectName); }
	String getReadableEffectDesc()   { return FPSTR(EffectRainbow::strReadableEffectDesc); }
	String toString() { return getReadableEffectName() + " (tick:" + tickInterval + "ms; loops:" + numLoops + ")"; }

	bool saveConfigToFile(String configPath);	// Stores current values of all variables related to the effect (so settings can be loaded on reboot)
	bool loadConfigFromJson(JsonObject& json);	// Loads effect settings from JSON buffer (already parsed)
	
	void resetCounters() { i = 0; }
	bool effectFunc();

protected:
	uint16_t i;
};

/**********************      EffectRainbowCycle      **********************/
class EffectRainbowCycle : public LedStripEffect {
public:
	EffectRainbowCycle(uint16_t tickInterval=defaultTickInterval, uint8_t numLoops=defaultNumLoops) : LedStripEffect(tickInterval, numLoops) {}
//	EffectRainbowCycle(String configPath) : EffectRainbowCycle() { loadConfigFromFile(configPath); }

	static const uint16_t defaultTickInterval;
	static const uint8_t  defaultNumLoops;
	
	static const char PROGMEM strCompressedEffectName[], strReadableEffectName[], strReadableEffectDesc[];	// Static constants to hold the unique string that getCompressedEffectName, etc. return
	String getCompressedEffectName() { return FPSTR(EffectRainbowCycle::strCompressedEffectName); }
	String getReadableEffectName()   { return FPSTR(EffectRainbowCycle::strReadableEffectName); }
	String getReadableEffectDesc()   { return FPSTR(EffectRainbowCycle::strReadableEffectDesc); }
	String toString() { return getReadableEffectName() + " (tick:" + tickInterval + "ms; loops:" + numLoops + ")"; }

	bool saveConfigToFile(String configPath);	// Stores current values of all variables related to the effect (so settings can be loaded on reboot)
	bool loadConfigFromJson(JsonObject& json);	// Loads effect settings from JSON buffer (already parsed)
	
	void resetCounters() { i = 0; }
	bool effectFunc();

protected:
	uint16_t i;
};

/**********************      EffectTheaterChase      **********************/
class EffectTheaterChase : public LedStripEffect {
public:
	EffectTheaterChase(RgbColor color=defaultColor, uint8_t step=defaultStep, uint16_t tickInterval=defaultTickInterval, uint8_t numLoops=defaultNumLoops) : LedStripEffect(tickInterval, numLoops), color(color), step(step) {}
//	EffectTheaterChase(String configPath) : EffectTheaterChase() { loadConfigFromFile(configPath); }

	RgbColor color;
	uint8_t step;
	static const RgbColor defaultColor;
	static const uint8_t  defaultStep;
	static const uint16_t defaultTickInterval;
	static const uint8_t  defaultNumLoops;

	static const char PROGMEM strCompressedEffectName[], strReadableEffectName[], strReadableEffectDesc[];	// Static constants to hold the unique string that getCompressedEffectName, etc. return
	String getCompressedEffectName() { return FPSTR(EffectTheaterChase::strCompressedEffectName); }
	String getReadableEffectName()   { return FPSTR(EffectTheaterChase::strReadableEffectName); }
	String getReadableEffectDesc()   { return FPSTR(EffectTheaterChase::strReadableEffectDesc); }
	String toString() { return getReadableEffectName() + " (c:[" + color.R + "," + color.G + "," + color.B + "]; step:" + step + "; tick:" + tickInterval + "ms; loops:" + numLoops + ")"; }

	bool saveConfigToFile(String configPath);	// Stores current values of all variables related to the effect (so settings can be loaded on reboot)
	bool loadConfigFromJson(JsonObject& json);	// Loads effect settings from JSON buffer (already parsed)
	
	void resetCounters() { i = 0; }
	bool effectFunc();

protected:
	uint16_t i;
};

/**********************      EffectTheaterChaseRainbow      **********************/
class EffectTheaterChaseRainbow : public LedStripEffect {
public:
	EffectTheaterChaseRainbow(uint8_t step=defaultStep, uint16_t tickInterval=defaultTickInterval, uint8_t numLoops=defaultNumLoops) : LedStripEffect(tickInterval, numLoops), step(step) {}
//	EffectTheaterChaseRainbow(String configPath) : EffectTheaterChaseRainbow() { loadConfigFromFile(configPath); }

	uint8_t step;
	static const uint8_t  defaultStep;
	static const uint16_t defaultTickInterval;
	static const uint8_t  defaultNumLoops;

	static const char PROGMEM strCompressedEffectName[], strReadableEffectName[], strReadableEffectDesc[];	// Static constants to hold the unique string that getCompressedEffectName, etc. return
	String getCompressedEffectName() { return FPSTR(EffectTheaterChaseRainbow::strCompressedEffectName); }
	String getReadableEffectName()   { return FPSTR(EffectTheaterChaseRainbow::strReadableEffectName); }
	String getReadableEffectDesc()   { return FPSTR(EffectTheaterChaseRainbow::strReadableEffectDesc); }
	String toString() { return getReadableEffectName() + " (step:" + step + "; tick:" + tickInterval + "ms; loops:" + numLoops + ")"; }

	bool saveConfigToFile(String configPath);	// Stores current values of all variables related to the effect (so settings can be loaded on reboot)
	bool loadConfigFromJson(JsonObject& json);	// Loads effect settings from JSON buffer (already parsed)
	
	void resetCounters() { i = j = 0; }
	bool effectFunc();

protected:
	uint16_t i, j;
};

/**********************      EffectVolumeShifter      **********************/
class EffectVolumeShifter : public LedStripEffect {
public:
	EffectVolumeShifter(uint32_t tEffectLength=defaultTeffectLength, uint16_t tickInterval=defaultTickInterval, uint8_t numLoops=defaultNumLoops) : LedStripEffect(tickInterval, numLoops), tEffectLength(tEffectLength) {}
//	EffectVolumeShifter(String configPath) : EffectVolumeShifter() { loadConfigFromFile(configPath); }

	static const uint32_t defaultTeffectLength;
	static const uint16_t defaultTickInterval;
	static const uint8_t  defaultNumLoops;

	static const char PROGMEM strCompressedEffectName[], strReadableEffectName[], strReadableEffectDesc[];	// Static constants to hold the unique string that getCompressedEffectName, etc. return
	String getCompressedEffectName() { return FPSTR(EffectVolumeShifter::strCompressedEffectName); }
	String getReadableEffectName()   { return FPSTR(EffectVolumeShifter::strReadableEffectName); }
	String getReadableEffectDesc()   { return FPSTR(EffectVolumeShifter::strReadableEffectDesc); }
	String toString() { return getReadableEffectName() + " (length:" + tEffectLength + "ms; loops:" + numLoops + ")"; }

	bool saveConfigToFile(String configPath);	// Stores current values of all variables related to the effect (so settings can be loaded on reboot)
	bool loadConfigFromJson(JsonObject& json);	// Loads effect settings from JSON buffer (already parsed)
	
	void resetCounters() { tDeadlineEffect = curr_time + tEffectLength; consolePrintf("%s set deadline for t=%lums\n", getReadableEffectName().c_str(), tDeadlineEffect); }
	bool effectFunc();

protected:
	uint32_t tEffectLength, tDeadlineEffect;
};

#endif

