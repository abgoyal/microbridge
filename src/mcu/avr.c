/*
Copyright 2011 Niels Brouwers

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.#include <string.h>
*/
#include "avr.h"
#include <stdint.h>

#define TIMER1_MULTIPLIER 64
#define TIMER1_MILLIS_DIVIDER (F_CPU / 1000L)
#define TIMER1_MICROS_DIVIDER (F_CPU / 1000000L)

// the prescaler is set so that timer0 ticks every 64 clock cycles, and the
// the overflow handler is called every 256 ticks.
#define MICROSECONDS_PER_TIMER0_OVERFLOW ((64 * 256) / (F_CPU / 1000000L))

// the whole number of milliseconds per timer0 overflow
#define MILLIS_INC (MICROSECONDS_PER_TIMER0_OVERFLOW / 1000)

// the fractional number of milliseconds per timer0 overflow. we shift right
// by three to fit these numbers into a byte. (for the clock speeds we care
// about - 8 and 16 MHz - this doesn't lose precision.)
#define FRACT_INC ((MICROSECONDS_PER_TIMER0_OVERFLOW % 1000) >> 3)
#define FRACT_MAX (1000 >> 3)

volatile uint32_t timer0_overflow_count = 0;
volatile uint32_t timer0_millis = 0;
static uint8_t timer0_fract = 0;
volatile uint16_t timer1_overflow_count = 0x0;

SIGNAL(TIMER0_OVF_vect)
{

	// copy these to local variables so they can be stored in registers
	// (volatile variables must be read from memory on every access)
	uint32_t m = timer0_millis;
	uint8_t f = timer0_fract;

	m += MILLIS_INC;
	f += FRACT_INC;
	if (f >= FRACT_MAX)
	{
		f -= FRACT_MAX;
		m += 1;
	}

	timer0_fract = f;
	timer0_millis = m;
	timer0_overflow_count ++;
}

unsigned long avr_millis()
{
	unsigned long m;
	uint8_t oldSREG = SREG;

	// disable interrupts while we read timer0_millis or we might get an
	// inconsistent value (e.g. in the middle of a write to timer0_millis)
	cli();
	m = timer0_millis;
	SREG = oldSREG;

	// Enable interrupts
	sei();

	return m;
}

uint64_t avr_ticks()
{
	return (timer0_overflow_count * 256 + TCNT0) * 64;
}

void avr_delay(unsigned long ms)
{
	unsigned long start = avr_millis();

	while (avr_millis() - start <= ms)
		;
}

uint32_t avr_micros()
{
	return ((uint32_t)(TCNT1 + ((uint32_t)timer1_overflow_count << 16)) * TIMER1_MULTIPLIER) / TIMER1_MICROS_DIVIDER;
}

/*
uint32_t avr_millis()
{
	return ((uint32_t)(TCNT1 + ((uint32_t)timer1_overflow_count << 16)) * TIMER1_MULTIPLIER) / TIMER1_MILLIS_DIVIDER;
}
*/

SIGNAL(TIMER1_OVF_vect)
{
	timer1_overflow_count ++;
}

void avr_timer1Init()
{
	// Initialise 16-bit timer 1
	// Set prescale to 64
	sbi(TCCR1B, CS01);
	sbi(TCCR1B, CS00);

	sbi(TIMSK1, TOIE1);
}

void avr_timerInit()
{
	// this needs to be called before setup() or some functions won't
	// work there
	sei();

	avr_timer1Init();

	// set timer 0 prescale factor to 64
	#ifdef __AVR_ATmega128__
	sbi(TCCR0, CS01);
	sbi(TCCR0, CS00);
	#else
	sbi(TCCR0B, CS01);
	sbi(TCCR0B, CS00);
	#endif
	


	// enable timer 0 overflow interrupt
	#ifdef __AVR_ATmega128__
	sbi(TIMSK, TOIE0);
	#else
	sbi(TIMSK0, TOIE0);
	#endif

	// set a2d prescale factor to 128
	// 16 MHz / 128 = 125 KHz, inside the desired 50-200 KHz range.
	// XXX: this will not work properly for other clock speeds, and
	// this code should use F_CPU to determine the prescale factor.
	/*
	sbi(ADCSRA, ADPS2);
	sbi(ADCSRA, ADPS1);
	sbi(ADCSRA, ADPS0);
	*/

}

// adapted from the wiring stuff
void avr_serialInit(uint32_t baud)
{
	uint16_t baud_setting;

	UCSR0A = 0;

	baud_setting = (F_CPU / 8 / baud - 1) / 2;
    // baud_setting = (CLOCKSPEED + (baud * 8L)) / (baud * 16L) - 1;

    // assign the baud_setting, a.k.a. ubbr (USART Baud Rate Register)
    UBRR0H = baud_setting >> 8;
    UBRR0L = baud_setting;

    sbi(UCSR0B, RXEN0);
    sbi(UCSR0B, TXEN0);
    sbi(UCSR0B, RXCIE0);
}

void avr_serialPrint(char * str)
{
	int i;
	for (i=0; str[i]!=0; i++)
		avr_serialWrite(str[i]);
}

void avr_serialVPrint(char * format, va_list arg)
{
	char temp[128];
	vsnprintf(temp, 128, format, arg);
	avr_serialPrint(temp);
}

void avr_serialPrintf(char * format, ...)
{
	va_list arg;

	va_start(arg, format);
	avr_serialVPrint(format, arg);
	va_end(arg);

}

void avr_serialWrite(unsigned char value)
{
	while (!((UCSR0A) & (1 << UDRE0)));
	UDR0 = value;
}
