#include <avr/io.h>      // this contains all the IO port definitions
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "util.h"
#include "main.h"

uint16_t EEMEM stored_number = 0;

const uint8_t numbertable[] PROGMEM = { 
	0x7E /* 0 */, 
	0x30 /* 1 */,
	0x6D /* 2 */,
	0x79 /* 3 */,
	0x33 /* 4 */,
	0x5B /* 5 */,
	0x5F, /* 6 */
	0x70, /* 7 */
	0x7F, /* 8 */
	0x73, /* 9 */
};
PGM_P numbertable_p PROGMEM = numbertable;

const uint8_t alphatable[] PROGMEM = {
	0x7D, /* a */
	0x1F, /* b */
	0x0D, /* c */
	0x3D, /* d */
	0x6F, /* e */
	0x47, /* f */
	0x7B, /* g */
	0x17, /* h */
	0x30, /* i */
	0x3C, /* j */
	0x57, //k
	0x0E, // l
	0x55, // m
	0x15, // n
	0x1D, // o
	0x67, //p
	0xF3, // q
	0x5, //r
	0x5B, //s
	0x0F, //t
	0x1C, //u
	0x1C, //v // fix?
	0x5C, //w
	0x37, //x
	0x3B, // y
	0x6d, //z
	/* more */
};
PGM_P alphatable_p PROGMEM = alphatable;

volatile uint8_t digit1, digit2, digit3;

volatile int16_t millisec_counter;
uint8_t clock_running;

volatile int16_t button_timer;
//#define SHORT_PRESS_TIME 50 // short click milliseconds
#define LONG_PRESS_TIME 1000 // long (press and hold) 3 seconds timeout

volatile uint16_t number;
#define DIGITS 3
volatile uint8_t current_digit_displayed = 0;
volatile uint8_t per_digit_timer;

volatile uint32_t poweroff_timer = 0;
#define POWEROFF_TIME 5 * 60 * 1000UL // 5 minutes * 60 seconds/min * 1000 millisec/sec

#define TIMER1RELOAD 550    // does not make any impact


//char *display_string = "abcdefghijklmnopqrstuvwxyz  ";
char *display_string = "   digg";
volatile uint8_t string_index = 0;
volatile uint16_t display_timer = 0;
#define SCROLL_SPEED 300UL

SIGNAL (SIG_INT0) 
{
  GIMSK = 0;
  poweroff_timer = 0;
  button_timer == 0xFFFF;
  while (! (BUTTON_PIN & _BV(BUTTON)))  {
    delay_ms(10);
  }
}

SIGNAL (SIG_OVERFLOW1)
{   
   TCNT1  = TIMER1RELOAD; // Reload timer with precalculated value
   millisec_counter++;
} 


SIGNAL (SIG_OUTPUT_COMPARE0A) 
{
  if (button_timer != 0xFFFF) 
  {
    button_timer++;
  }
  
  if (display_timer != 0xFFFF) 
  {
    display_timer++;
    if (display_timer >= SCROLL_SPEED) 
	{
      display_timer = 0;
      string_index++;
      if (string_index > strlen(display_string)+3)
	    string_index -= strlen(display_string);
    }
  }

  if (PINA & 0x1) 
  {	
    if (poweroff_timer < POWEROFF_TIME) 
	{
      poweroff_timer++;
    } 
	else 
	{
      // time to sleep!
      // turn off all LEDs
      
	  shutdown();
    }
  } // otherwise, its always on!
  
  
  if ((PINA & 0x2) && (number <= 999))  /* display whatever is stored in the number variable */
  {
    // display a number
    if (number >= 100) {
      digit1 = hundreds(number);
    } else {
      digit1 = ' ';
    }
    
    if (number >= 10) {
      digit2 = tens(number);
    } else {
      digit2 = ' ';
    }
    
    digit3 = number % 10;
  } else {
    // do whatever the main code sez
  }
  
  per_digit_timer--;
  if (per_digit_timer == 0) {		
    current_digit_displayed++;
    if (current_digit_displayed >= 3)
      current_digit_displayed = 0;
    
    if (current_digit_displayed == 0) { 
      if (segments(digit1) == 0) {
	SEG_CC1_PORT |= _BV(SEG_CC1);
	current_digit_displayed = 1;
      } else {
	display_digit(0, digit1);
	per_digit_timer = segments(digit1);
      }
    }
    if (current_digit_displayed == 1) {
      if (segments(digit2) == 0) {
	SEG_CC2_PORT |= _BV(SEG_CC2);
	current_digit_displayed = 2;
      } else {
	display_digit(1, digit2);
	per_digit_timer = segments(digit2);
      }
    } 
    if (current_digit_displayed == 2) {
      if (segments(digit3) == 0) {
	SEG_CC3_PORT |= _BV(SEG_CC3);
	per_digit_timer = 1;
      } else {
	display_digit(2, digit3);
	per_digit_timer = segments(digit3);
      }
    }
  }
}


void shutdown()
{
      number = 0;
	  millisec_counter = 0;
	  clock_running = 0;	
	  display_digit(9, 99);
      MCUCR |= _BV(SM1) | _BV(SM0) | _BV(SE);
      GIMSK = _BV(INT0);
      sei();
      asm("sleep");
}


void display_one(uint8_t d) {
	if (d & 0x1)  // G 
		PORTD |= _BV(SEG_G);
	else
		PORTD &= ~_BV(SEG_G);

	if (d & 0x2)  // F 
		PORTB  |= _BV(SEG_F);
	else
		PORTB &= ~_BV(SEG_F);

	if (d & 0x4)  // E
		PORTB  |= _BV(SEG_E);
	else
		PORTB &= ~_BV(SEG_E);

	if (d & 0x8)  // D
		PORTB  |= _BV(SEG_D);
	else
		PORTB &= ~_BV(SEG_D);

	if (d & 0x10)  // C
		PORTB  |= _BV(SEG_C);
	else
		PORTB &= ~_BV(SEG_C);

	if (d & 0x20)  // B
		PORTB  |= _BV(SEG_B);
	else
		PORTB &= ~_BV(SEG_B);

	if (d & 0x40)  // A
		PORTB  |= _BV(SEG_A);
	else
		PORTB &= ~_BV(SEG_A);

	if (d & 0x80)  // dot
		PORTD  |= _BV(SEG_DP);
	else
		PORTD &= ~_BV(SEG_DP);
}

void display_digit(uint8_t digit, uint8_t value) {

	// turn them all off (to prevent ghosting)
	SEG_CC1_PORT |= _BV(SEG_CC1);
	SEG_CC2_PORT |= _BV(SEG_CC2);
	SEG_CC3_PORT |= _BV(SEG_CC3);
	SEG_DCC_PORT |= _BV(SEG_DCC);	

	// set the anodes
	if (value <= 9) {
	  display_one(pgm_read_byte(numbertable_p + value));
	} else if ((value >= 'a') && (value <= 'z')) {
	  display_one(pgm_read_byte(alphatable_p + value - 'a'));
	} else if (value == ' ') {
		display_one(0x0);
	}
	
	// turn on cathodes
	if (digit == 0) {
		SEG_CC1_PORT &= ~_BV(SEG_CC1);
	} else if (digit == 1) {
		SEG_CC2_PORT &= ~_BV(SEG_CC2);
	} else if (digit == 2) {
		SEG_CC3_PORT &= ~_BV(SEG_CC3);
	} else if (digit == 3) {
		SEG_DCC_PORT &= ~_BV(SEG_DCC);
	}
}

uint8_t segments(uint8_t value) {
	int x, i, j;

	if (value <= 9) {
	  x = pgm_read_byte(numbertable_p + value);
	} else if ((value >= 'a') && (value <= 'z')) {
	  x = pgm_read_byte(alphatable_p + value - 'a');
	} else {
	  return 0x0;
	}
	
	j = 0;

	for (i=0; i<8; i++) {
		if (x & 0x1)
			j++;
		x >>= 1;
	}
	return j;
}

int main(void) {
  uint8_t button_state, last_button_state, cleared = 0;

  DDRB = 0xFF;
  DDRD = 0xFF;
  DDRA = 0x0; // the unused crystal pins are for changing the timeout.
  PORTA = 0xF; // but they have pullups
  
  GIMSK = 0;
  
  BUTTON_DDR &= ~_BV(BUTTON);
  BUTTON_PORT |= _BV(BUTTON);
  
  
  // set up an interrupt that goes off @ 1KHz) (internal clock is 8MHz)
  TCCR0A = _BV(WGM01);                          //timer control register A
  TCCR0B = 2; // 500K/8 = 62.3KHz               //timer control register B  //Cs2 per 8
  OCR0A = 63; // divide that by 63 -> 1KHz      //ouptut compare
  TIMSK |= _BV(OCIE0A); // turn on the interrupt

  //Set up second timer 
  TCNT1 = TIMER1RELOAD; // Preload timer with precalculated value
  TCCR1A = _BV(WGM01); 
  TCCR1B |= ((1 << CS10)); // Set up timer at Fcpu/64 
  //TCCR1B |= ((1 << CS10) | (1 << CS11)); // Set up timer at Fcpu/64 
  TIMSK |= (1 << TOIE1); // Enable overflow interrupt 

  sei(); // turn on interrupts
  // now all the LED stuff (persistence of vision) is done in interrupts, so watch for button presses
  
  button_timer = 0xFFFF;
  last_button_state = button_state = 1;

  millisec_counter = 0;
  clock_running = 1;
  number = 0;

  while (1) {
    // first, debounce the switch by performing a lowpass filter: the button
    // must be pressed for 10ms (on or off) for it to count. otherwise we get
    // weird effects from the bouncing.

    if ((! (BUTTON_PIN & _BV(BUTTON))) && (button_state == 1)) {
      poweroff_timer = 0;

      //button was just pressed
      // wait 10 ms and see if the button is still pressed the entire time
      button_timer = 0;
      while (button_timer < 10) {
	if (BUTTON_PIN & _BV(BUTTON)) { // released
	  break;
	}
      }
      if (button_timer >= 10) { // was held entire time
	button_state = 0;
      }
      cleared = 0;
    }
    
    if ((BUTTON_PIN & _BV(BUTTON)) && (!button_state)) {
      poweroff_timer = 0;

      // wait 10 ms and see if the button is released the entire time
      button_timer = 0;
      while (button_timer < 10) {
	if (! (BUTTON_PIN & _BV(BUTTON))) { // pressed
	  break;
	}
      }
      if (button_timer >= 10) { // was held entire time
	button_state = 1;
      }
    }

    // debounce/lowpassing done, 
    // button_state tells whether the button is pressed (0) or released (1)
    // now see if it was a short or long press
    
	if (!button_state && last_button_state)  // just pressed
    {
	  last_button_state = button_state;
      // do nothing. Not distinguishing button down, as hold will come after 3 sec it went down,
    }
   
    if (!button_state && !last_button_state && (button_timer >= LONG_PRESS_TIME))  // pressed and held
    {
	  if (number == 0)  // send it to sleep
	  {
	  	 //shutdown();   // not working yet TODO
	  }
	  number = 0;
	  clock_running = 0;
      cleared = 1; // just cleared
    }

    if (button_state && !last_button_state && !cleared)  // just released
	{ 
		if (clock_running == 1) 
		{
			clock_running = 0;
		}	
		else 
		{
			millisec_counter = 0;
			clock_running = 1;			
		}
		cleared = 1; // just cleared
    }


	if (clock_running > 0) 
	{		
		if (millisec_counter > 852) 
		{
			millisec_counter = 0;
			number++;			
		}
	}
  }
}

