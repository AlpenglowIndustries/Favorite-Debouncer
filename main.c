/*
Carrie Sundra's Favorite Button Debouncer.
Right now.  So far.
Processor: ATMega328P

What I like:
- it's intended for use with a bouncy-ass arcade button with internal LED
- button is hooked straight up to an input pin with external pull-up resistor
- it's pretty easy to understand what's going on,
  tho sorry Arduino folks,
  this is written in straight C with low-level Atmel libs
  I'll try to add an Arduino version at some point.
- it's configurable
- it fuckin' works!

 */

 // Oh you're gonna hate me here too, my fuse settings are different from most Arduinos
 // I just use the internal clock and run it with the /8 divider so it's 1 MHz
 //
 // FUSE SETTINGS (for the 328P):
 // LOW:  0x62 (default)
 // HIGH: 0xD1 EESAVE enabled (does not overwrite EEPROM on reprogramming, not important for this example)
 // EXT:  0x04 (or 0xFC) BOD enabled @ 4.3V

#define F_CPU	1000000ul
#define VERSION	"0.1"

#include <avr/io.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <avr/interrupt.h>

// if you're using different pins, you'll have to change the defs here and
//   the port letters in initOutputs or initButt
// BRB = Big Red Button as in one of those 60mm arcade buttons
#define LED     PORTD6
#define BUTT    PORTD2
#define BRBLED  PORTD5

// yeah yeah there are other ways of doing the below, I know
#define TOGGLE_LED	  (PORTD ^= (1 << LED))
#define SET_LED       (PORTD |= (1 << LED))
#define CLEAR_LED     (PORTD &= ~(1 << LED))
#define TOGGLE_BRBLED	(PORTD ^= (1 << BRBLED))
#define SET_BRBLED		(PORTD |= (1 << BRBLED))
#define CLEAR_BRBLED	(PORTD &= ~(1 << BRBLED))
#define BUTTISPRESSED	bit_is_clear(PIND, BUTT)
#define BUTTISRELEASED	bit_is_set(PIND, BUTT)
#define MINREADTIME    5  // multiplied by the number of valid lows is the minimum debounce time
#define NUMLOWS        5  // number of valid lows you want to detect
#define BOUNCENUM      (2^NUMLOWS)  // the actual number checked for in the debounce routine

//---------ISR Variables--------------
volatile uint32_t milliseconds = 0;
volatile uint8_t buttPress = 0;
volatile uint32_t buttTime = 0;

static inline void initOutputs (void) {
  // sets the following to outputs
  // everything else is set to an input by default on the ATMega328P
	DDRD |= (1 << LED) | (1 << BRBLED);
}

static inline void initMillisTimer (void) {
	// 8-bit Timer0 used for Arduino millis, may as well keep compatible
	TCCR0A |= (1 << WGM01);					// sets CTC mode
	OCR0A = 125;							// 125 = 1 ms @ F_CPU = 1MHz
	TIMSK0 |= (1 << OCIE0A);				// enables interrupt
	TCCR0B |= (1 << CS01);					// starts timer with prescalar: F_CPU/8 (125,000), no output waveforms
}

static inline void initButt (void) {
//  PORTD |= (1 << BUTT);			// if you want to use the internal pull-up resistor for the arcade button, not tested but should be right
	EICRA |= (1 << ISC01);			// INT0 triggers on falling edge
	EIMSK |= (1 << INT0);			// enables INT0
};

uint32_t millis(void) {
	uint32_t mscopy;
	ATOMIC_BLOCK(ATOMIC_FORCEON) {  // if you don't know about atomic operations, look them up
		mscopy = milliseconds;
	}
	return mscopy;
}

// THE BUSINESS:
// Assumes a button that is GNDed when pressed
// Looks for NUMLOWS (default = 5) number of valid low readings (converted to a single 8-bit int in BOUNCENUM, 0b11111 by default)
//   in a row after the button interrupt fires
// Makes sure MINREADTIME (default = 5 ms) has passed between each reading
// Ensures a minimum debounce time of 25 ms (default)
// Min debounce time may be unnecessary, but arcade buttons are bouncy little bitches
uint8_t checkButt() {
	static uint8_t buttFlags = 0; // static is important - we want to preserve the variable between function calls
	EIMSK &= ~(1 << INT0);				            // disables INT0 interrupt to guarantee clearing buttPress
	if (buttPress && (millis() - buttTime > MINREADTIME)) { // checks if the interrupt fired, makes sure there's a reasonable minimum debounce time
		if (BUTTISPRESSED) {                        // checks the actual button reading - if it's not still pressed, it's a bounce, so GTFO
			buttFlags = (buttFlags << 1) | 1;       // shifts the 1's over and adds another one at the end
		}
		else buttFlags = 0;       // just wipes out everything if the interrupt fired
	}
	EIMSK |= (1 << INT0);		  // enables INT0 interrupt
	if (buttFlags >= BOUNCENUM) {     // = 31 = 0x1F = 0b11111 = 5 low readings (default) in a row after a button press
		buttFlags = 0;                // clears variables for next go at debouncing
		buttPress = 0;
		return 1;                     // congratulations, you have a valid button press!
	}
	else return 0;                // awwww, maybe next time
}

// purposefully kept short and sweet
ISR(INT0_vect) {
	buttTime = millis();
	buttPress = 1;
}

// your generic millisecond timer
ISR(TIMER0_COMPA_vect) {
	milliseconds++;
}

int main(void)
{
	uint32_t ledBlinkTime = 0;
	initOutputs();
	initMillisTimer();
	initButt();

    while (1)
    {
    	uint8_t button = checkButt();
    	if (button) TOGGLE_BRBLED;  // toggles the LED on the arcade button

        // blinks another LED for a heartbeat
    	if (millis() - ledBlinkTime > 500)	{
    		TOGGLE_LED;
    		ledBlinkTime = millis();
    	}
    }
}
