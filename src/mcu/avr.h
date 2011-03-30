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

/**
 * \brief Misc. functions for the atmel AVR series of micro controllers.
 *
 * This small library supplies basic methods to work with the Atmega series of micro controllers. It contains a millisecond timer and uart functions.
 *
 * The library is based on the work of Pascal Stang's AVRLib [1].
 *
 * [1] http://www.mil.ufl.edu/~chrisarnold/components/microcontrollerBoard/AVR/avrlib/
 *
 */

#ifndef __avr_h__
#define __avr_h__

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

// TODO: remove this
typedef uint8_t boolean;
typedef uint8_t byte;

// Default clock speed to 16Mhz
#ifndef F_CPU
#define F_CPU 16000000
#endif

// clear bit, set bit macros
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

// Timer routines

/**
 * Initalises the AVR timer 0. Must be called before using avr_millis, avr_ticks, or avr_miros
 */
void avr_timerInit();

/**
 * @return milliseconds passed since timer initialisation.
 */
uint32_t avr_millis();


/**
 * @return clock ticks passed since timer initialisation.
 */
uint64_t avr_ticks();

/**
 * @return micro seconds passed sicne timer initialisation.
 */
uint32_t avr_micros();

/**
 * Busy-wait for a given number of milliseconds.
 * @param ms number of milliseconds to wait.
 */
void avr_delay(unsigned long ms);

/**
 * Set up serial port 0.
 * @param baud desired baud rate (i.e. 576000.
 */
void avr_serialInit(uint32_t baud);

/**
 * Print a character string to the serial port.
 * @param str string to print.
 */
void avr_serialPrint(char * str);

/**
 * Serial printf.
 * @param format printf format string.
 */
void avr_serialPrintf(char * format, ...);

/**
 * Print a single byte to the serial port.
 * @param value
 */
void avr_serialWrite(uint8_t value);

#endif
