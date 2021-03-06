/******      GPIO      ******/
#ifndef GPIO_H_
#define GPIO_H_

#include "main.h"				// HotTub global includes and definitions
#include <Wire.h>				// I2C library (GPIO expander)
#include <SPI.h>				// SPI library (external ADC)
#include <ESPAsyncWebServer.h>	// HTTP web server to handle requests to turn on/off lights, sound, etc.

#define GPIO_EXP_ADDR	0x20	// Only last 3 bits of address could be changed (0x20-0x27). Currently all bits are shorted to GND, so 0x20
#define GPIO_EXP_IODIRA	0x00	// IODIRA register controls IO direction for port A: 0=output, 1=input
#define GPIO_EXP_IODIRB	0x01	// IODIRB register controls IO direction for port B: 0=output, 1=input
#define GPIO_EXP_GPPUA	0x0C	// GPPUA register controls PullUp resistors for input pins in port A: 1=PullUp enabled, 0=disabled
#define GPIO_EXP_GPPUB	0x0C	// GPPUB register controls PullUp resistors for input pins in port B: 1=PullUp enabled, 0=disabled
#define GPIO_EXP_PORTA	0x12	// Command to select Port A (to either write new values to output pins or read inputs)
#define GPIO_EXP_PORTB	0x13	// Command to select Port B (to either write new values to output pins or read inputs)
#define RELAY_LIGHTS1	4
#define RELAY_LIGHTS2	5
#define RELAY_MUSIC		6
#define PWMRANGE		1023
#define ADC_BUF_SIZE	1000

extern byte gpioExpPortA, gpioExpPortB;		// Local copy of last known status of MCP23017's PORTA and PORTB
extern byte relayStatus;					// (Active-low) relay control signal, decides which relays to turn on/off
extern uint8_t audioKnobInCh, audioOutCh;	// Selected channel input in the audio knob and desired channel output
extern uint16_t adc_buf[2][ADC_BUF_SIZE];	// ADC data buffer, double buffered
extern unsigned int adc_buf_id_current;		// Which data buffer is being used for the ADC (the other is being sent)
extern unsigned int adc_buf_pos;			// Position (index) in the ADC data buffer (index in adc_buf[adc_buf_id_current])
extern bool adc_buf_got_full;				// Flag to signal that a buffer is ready to be sent


/***************************************************/
/******            SETUP FUNCTIONS            ******/
/***************************************************/
void setupIOpins();			// Setup all IO pins, including on-board pins and MCP23017
void setupTimer1();			// Setup Timer1 so we can sample the ADC on a 10kHz interrupt
static inline void setDataBits(uint16_t bits);	// Sets up SPI with the given number of data bits
void setupExternalADC();	// Configure SPI to communicate with the external ADC (MCP3201)
void setupGPIOexpander();	// Setup MCP23017


/**********************************************/
/******      GPIO related functions      ******/
/**********************************************/
byte reverseByte(byte in);						// Helper function that flips left-right a byte (so 01001111 becomes 11110010 and so on)
uint8_t findSetBitInByte(byte in);				// Helper function that returns the index of the first bit that's set (ie, equals 1) in a byte. Right-most bit is 0, left-most is 7, not found is 8.
byte readGPIOexpPort(byte port);				// Read the value of a port/register (eg, GPIO_EXP_PORTA would read PORTA) from the MCP23017
void writeGPIOexpPort(byte port, byte value);	// Write a value to the port/register (eg, GPIO_EXP_PORTA would write to PORTA) on the MCP23017
uint8_t getAudioKnobSelectedCh();				// Find out which channel is the audio knob selecting
void sendAudioSelectedCh();						// Write to MCP23017's PORTA the appropriate value based on desired audioOutCh
void setRelay(uint8_t num, bool setOn);			// Turn on/off the num-th relay
static inline ICACHE_RAM_ATTR uint16_t transfer16();	// Read 16 bits from SPI
void ICACHE_RAM_ATTR sample_isr();				// Samples the ADC and pushes the value to the adc_buf (switches to a new buffer if full)
void processGPIO();								// "GPIO.loop()" function: reads inputs, processes them and writes outputs

/**** Dirty way to get Relay control until MCP23017 arrives (START) ****/
extern bool lightsOn, soundOn;
void turnReplyHtml(uint8_t turnWhat, bool state, AsyncWebServerRequest *request);
void turnLights(bool on, AsyncWebServerRequest* request=NULL);
void turnSound(bool on, AsyncWebServerRequest* request=NULL);
void turnRelays(bool on, AsyncWebServerRequest* request=NULL);
/**** Dirty way to get Relay control until MCP23017 arrives (END) ****/

#endif

