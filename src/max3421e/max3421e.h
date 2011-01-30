/**
 *
 * Library for the max3421e USB host controller shield produced by circuitsathome and Sparkfun.
 * This is a low-level interface that provides access to the internal registers and polls the
 * controller for state changes.
 *
 * This library is based on work from Oleg, but has been ported to C and heavily restructured.
 * Control over the GPIO pins has been stripped.
 *
 * Note that the current incarnation of this library only supports the Arduino Mega with a
 * hardware mod to rewire the MISO, MOSI, and CLK SPI pins.
 *
 * http://www.circuitsathome.com/
 */
#ifndef _MAX3421E_H_
#define _MAX3421E_H_

#include <stdbool.h>
#include <stdint.h>

#include "../spi.h"
#include "max3421e_constants.h"

/**
 * Max3421e registers in host mode.
 */
enum
{
	RCVFIFO = 0x08,
	SNDFIFO = 0x10,
	SUDFIFO = 0x20,
	RCVBC = 0x30,
	SNDBC = 0x38,
	USBIRQ = 0x68,
	USBIEN = 0x70,
	CPUCTL = 0x80,
	USBCTL = 0x78,
	PINCTL = 0x88,
	REVISION = 0x90,
	FNADDR = 0x98,
	GPINIRQ = 0xb0,
	HIRQ = 0xc8,
	HIEN = 0xd0,
	MODE = 0xd8,
	PERADDR = 0xe0,
	HCTL = 0xe8,
	HXFR = 0xf0,
	HRSL = 0xf8
} max_registers;

// Definitions for the ports and bitmasks required to drive the various pins.
#define MAX_PORT_SS PORTB
#define MAX_PORT PORTH
#define MAX_PORT_DIR DDRH

#define MAX_BIT_INT 0x40
#define MAX_BIT_GPX 0x20
#define MAX_BIT_RESET 0x10
#define MAX_BIT_SS 0x10

#define MAX_SHIFT_INT 6
#define MAX_SHIFT_GPX 5

#define MAX_INT() ((MAX_PORT & MAX_BIT_INT) >> MAX_SHIFT_INT)
#define MAX_GPX() ((MAX_PORT & MAX_BIT_GPX) >> MAX_SHIFT_GPX)
#define MAX_RESET(x) { if (x) MAX_PORT |= MAX_BIT_RESET; else MAX_PORT &= ~MAX_BIT_RESET; }
#define MAX_SS(x) { if (x) MAX_PORT_SS |= MAX_BIT_SS; else MAX_PORT_SS &= ~MAX_BIT_SS; }

void max3421e_init();
void max3421e_write(uint8_t reg, uint8_t val);
char * max3421e_writeMultiple(uint8_t reg, uint8_t nuint8_ts, char * data);
void max3421e_gpioWr(uint8_t val);
uint8_t max3421e_read(uint8_t reg);
char * max3421e_readMultiple(uint8_t reg, uint8_t nuint8_ts, char * data);
uint8_t max3421e_gpioRd(void);
boolean max3421e_reset();
boolean max3421e_vbusPwr(boolean action);
void max3421e_busprobe(void);
void max3421e_powerOn();
uint8_t max3421e_getVbusState();

uint8_t max3421e_poll(void);

uint8_t max3421e_IntHandler(void);
uint8_t max3421e_GpxHandler(void);

#endif //_MAX3421E_H_
