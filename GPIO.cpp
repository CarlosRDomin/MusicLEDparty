/******      GPIO      ******/
#include "GPIO.h"

byte gpioExpPortA = 0, gpioExpPortB = 0;
byte relayStatus = 0xFF;	// Relays are active-low, so let's start with all the relays off
uint8_t audioKnobInCh = 0, audioOutCh = 0;

uint16_t adc_buf[2][ADC_BUF_SIZE];		// ADC data buffer, double buffered
unsigned int adc_buf_id_current = 0;	// Which data buffer is being used for the ADC (the other is being sent)
unsigned int adc_buf_pos = 0;			// Position (index) in the ADC data buffer (index in adc_buf[adc_buf_id_current])
bool adc_buf_got_full = false;			// Flag to signal that a buffer is ready to be sent

#define TEMP_PIN_LIGHTS1	14	// 15
#define TEMP_PIN_LIGHTS2	12	// 13
#define TEMP_PIN_SOUND		13	// 12


/***************************************************/
/******            SETUP FUNCTIONS            ******/
/***************************************************/
void setupIOpins() {	// Setup all IO pins, including on-board pins and MCP23017
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LOW);	// Enciende el LED para indicar que estoy iniciando...
	analogWriteRange(PWMRANGE);

	// Setup on-board pins
	pinMode(TEMP_PIN_LIGHTS1, OUTPUT);
	pinMode(TEMP_PIN_LIGHTS2, OUTPUT);
	pinMode(TEMP_PIN_SOUND, OUTPUT);
	turnLights(false);
	turnSound(false);

	// Setup external peripherals
	setupExternalADC();		// Setup MCP3201
	setupGPIOexpander();	// Setup MCP23017
}

void setupTimer1() {	// Setup Timer1 so we can sample the ADC on a 10kHz interrupt
	timer1_isr_init();
	timer1_attachInterrupt(sample_isr);
	timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
	timer1_write(clockCyclesPerMicrosecond() / 32 * 100); //100us = 10kHz sampling freq
}

static inline void setDataBits(uint16_t bits) {	// Sets up SPI with the given number of data bits
    const uint32_t mask = ~((SPIMMOSI << SPILMOSI) | (SPIMMISO << SPILMISO));
    bits--;
    SPI1U1 = ((SPI1U1 & mask) | ((bits << SPILMOSI) | (bits << SPILMISO)));
}

void setupExternalADC() {	// Configure SPI to communicate with the external ADC (MCP3201)
	SPI.begin();
	SPI.setDataMode(SPI_MODE0);
	SPI.setBitOrder(MSBFIRST);
	SPI.setClockDivider(SPI_CLOCK_DIV8); 
	SPI.setHwCs(1);
	setDataBits(16);
	setupTimer1();	// And start the timer to sample ADC
}

void setupGPIOexpander() {	// Setup MCP23017
	Wire.begin();	// Initialize I2C library
	
	// Configure the values of IODIRA and IODIRB for the I2C GPIO port expander (0=output, 1=input)
	writeGPIOexpPort(GPIO_EXP_IODIRA, 0x0F);	// A7-A4 -> Selected audio ch; A3-A0 -> Input from audio knob in the front
	writeGPIOexpPort(GPIO_EXP_IODIRB, 0x00);	// B7-B0 -> Relay control (output)
	
	writeGPIOexpPort(GPIO_EXP_GPPUA,0x0F);		// Enable pull-up resistors for A3-A0 (audio knob inputs)
}


/**********************************************/
/******      GPIO related functions      ******/
/**********************************************/
byte reverseByte(byte in) {	// Helper function that flips left-right a byte (so 01001111 becomes 11110010 and so on)
	static byte lookup[16] = {
		0b0000, 0b1000, 0b0100, 0b1100, 0b0010, 0b1010, 0b0110, 0b1110,	// Index 1=0b0001 => 0b1000
		0b0001, 0b1001, 0b0101, 0b1101, 0b0011, 0b1011, 0b0111, 0b1111};// Index 8==0b1000 => 0b0001
	return (lookup[in&0xF]<<4) | lookup[in>>4];	// Reverse top and bottom nibbles and then swap them
}

uint8_t findSetBitInByte(byte in) {	// Helper function that returns the index of the first bit that's set (ie, equals 1) in a byte. Right-most bit is 0, left-most is 7, not found is 8.
	for (uint8_t i=0; i<8; ++i) {
		if (in & 0x1) {
			return i;
		} else {
			in = in >> 1;
		}
	}
	return 8;
}

byte readGPIOexpPort(byte port) {	// Read the value of a port/register (eg, GPIO_EXP_PORTA would read PORTA) from the MCP23017
	Wire.beginTransmission(GPIO_EXP_ADDR);
	Wire.write(port);
	Wire.endTransmission();
	Wire.requestFrom(GPIO_EXP_ADDR, 1);
	return Wire.read();
}

void writeGPIOexpPort(byte port, byte value) {	// Write a value to the port/register (eg, GPIO_EXP_PORTA would write to PORTA) on the MCP23017
	Wire.beginTransmission(GPIO_EXP_ADDR);
	Wire.write(port);
	Wire.write(value);
	Wire.endTransmission();
}

uint8_t getAudioKnobSelectedCh() {	// Find out which channel is the audio knob selecting
	audioKnobInCh = findSetBitInByte(~gpioExpPortA & 0x0F);	// Audio knob is connected on the lower 'nibble' of PORTA (A3-A0) and is active-low (so we want to find which input is 0)
	return audioKnobInCh;
}

void sendAudioSelectedCh() {	// Write to MCP23017's PORTA the appropriate value based on desired audioOutCh
	gpioExpPortA = (~(0x10 << audioOutCh) & 0xF0) | (gpioExpPortA & 0x0F);
	writeGPIOexpPort(GPIO_EXP_PORTA, gpioExpPortA);
}

void setRelay(uint8_t num, bool setOn) {	// Turn on/off the num-th relay
	// num is the relay # whose value to be set. Starts at 0 (range is 0-7 for now)
	if (setOn) {	// Relays are active-low, need to write a '0' in bit 'num'
		relayStatus &= ~(1<<num);
	} else {		// Write a '1'
		relayStatus |= (1<<num);
	}
}

/**** Dirty way to get Relay control until MCP23017 arrives (START) ****/
bool lightsOn = false, soundOn = false;
enum {TURN_LIGHTS, TURN_SOUND, TURN_RELAYS};

void turnReplyHtml(uint8_t turnWhat, bool state, AsyncWebServerRequest* request) {
	if (!request) return;
	
	static const char* const PROGMEM turnWhat_P[] = {"Lights ", "Sound ", "Relays "};
	static const char* const PROGMEM url_P[] = {"lights_", "sound_", ""};
	static const char* const PROGMEM state_P[] = {"off", "on"};
	static const char* const PROGMEM plural_P[] = {"it", "them"};

	/* Example response: Lights on!<br><a href='lights_off'>Click here to turn them back off</a> */
	AsyncWebServerResponse* response = request->beginResponse(200, contentType_P[TYPE_HTML], SFPSTR(turnWhat_P[turnWhat]) + FPSTR(state_P[state]) + F("!<br><a href='") + FPSTR(url_P[turnWhat]) + FPSTR(state_P[!state]) + F("'>Click here to turn ") + FPSTR(plural_P[turnWhat!=1]) + F("back ") + FPSTR(state_P[!state]) + F("</a>"));
	addNoCacheHeaders(response);	// Don't cache so if they want to turn lights/sound on/off the browser sends a new request
	request->send(response);
}

void turnLights(bool on, AsyncWebServerRequest* request) {
	lightsOn = on;
	digitalWrite(TEMP_PIN_LIGHTS1, !on);
	digitalWrite(TEMP_PIN_LIGHTS2, !on);
	setRelay(RELAY_LIGHTS1, on);
	setRelay(RELAY_LIGHTS2, on);

	turnReplyHtml(TURN_LIGHTS, on, request);
}

void turnSound(bool on, AsyncWebServerRequest* request) {
	soundOn = on;
	digitalWrite(TEMP_PIN_SOUND, !on);
	setRelay(RELAY_MUSIC, on);

	turnReplyHtml(TURN_SOUND, on, request);
}

void turnRelays(bool on, AsyncWebServerRequest* request) {
	turnLights(on);
	turnSound(on);
	
	turnReplyHtml(TURN_RELAYS, on, request);
}
/**** Dirty way to get Relay control until MCP23017 arrives (END) ****/

static inline ICACHE_RAM_ATTR uint16_t transfer16() {	// Read 16 bits from SPI
	union {
		uint16_t val;
		struct {
			uint8_t lsb;
			uint8_t msb;
		};
	} out;

	// Transfer 16 bits at once, leaving HW CS low for the whole 16 bits 
	while(SPI1CMD & SPIBUSY) {}
	SPI1W0 = 0;
	SPI1CMD |= SPIBUSY;
	while(SPI1CMD & SPIBUSY) {}

	/* Follow MCP3201's datasheet: return value looks like this:
	xxxBA987 65432101
	We want 
	76543210 0000BA98
	So swap the bytes, select 12 bits starting at bit 1, and shift right by one.
	*/

	out.val = SPI1W0 & 0xFFFF;
	uint8_t tmp = out.msb;
	out.msb = out.lsb;
	out.lsb = tmp;

	out.val &= (0x0FFF << 1);
	out.val >>= 1;
	return out.val;
}

void ICACHE_RAM_ATTR sample_isr() {	// Samples the ADC and pushes the value to the adc_buf (switches to a new buffer if full)
	// Read a sample from ADC and write it to the buffer
	uint16_t val = transfer16();	// Read ADC (through SPI)
	adc_buf[adc_buf_id_current][adc_buf_pos] = val;	// Save the value in the buffer
	adc_buf_pos++;	// And increase the buffer cursor

	// If the buffer is full, switch to the other one and signal that it's ready to be sent
	if (adc_buf_pos > sizeof(adc_buf[0])/sizeof(adc_buf[0][0])) {
		adc_buf_pos = 0;
		adc_buf_id_current = !adc_buf_id_current;
		adc_buf_got_full = true;
	}
}

void processGPIO() {	// "GPIO.loop()" function: reads inputs, processes them and writes outputs
	// Blink built-in LED to show we're alive :)
	//analogWrite(LED_BUILTIN, (((curr_time>>10) & 0x01)? curr_time:~curr_time) & 0x3FF);	// Fading heartbeat every ~1s [for binary heartbeat use instead: digitalWrite(LED_BUILTIN, (curr_time>>10) & 0x01);]
	digitalWrite(LED_BUILTIN, (curr_time>>10) & 0x01);	// Analog write uses CPU to fake a PWM -> Because we use high CPU it doesn't work -> Use traditional "binary" toggle
	
	// Read inputs (audio knob only for now)
	gpioExpPortA = readGPIOexpPort(GPIO_EXP_PORTA);
	// gpioExpPortB = readGPIOexpPort(GPIO_EXP_PORTB);	// No need to read PORTB, it's all outputs

	// Process audio knob. Only change audioOutCh if knob position changed. No debouncing needed
	uint8_t lastAudioKnobInCh = audioKnobInCh;
	if (getAudioKnobSelectedCh() != lastAudioKnobInCh) {
		audioOutCh = audioKnobInCh;
	}
	sendAudioSelectedCh();	// Send the right output so selected audio channel is audioOutCh

	// Now send relay status (PortB)
	writeGPIOexpPort(GPIO_EXP_PORTB, relayStatus);
}

