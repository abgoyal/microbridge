/*
 * Copyright (c) 2010 by Cristian Maglie <c.maglie@bug.st>
 * SPI Master library for arduino.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#ifndef _SPI_H_INCLUDED
#define _SPI_H_INCLUDED

#include "avr.h"

#define SPI_CLOCK_DIV4 0x00
#define SPI_CLOCK_DIV16 0x01
// #define SPI_CLOCK_DIV64 0x02
#define SPI_CLOCK_DIV128 0x03
#define SPI_CLOCK_DIV2 0x04
#define SPI_CLOCK_DIV8 0x05
#define SPI_CLOCK_DIV32 0x06
#define SPI_CLOCK_DIV64 0x07

#define SPI_MODE0 0x00
#define SPI_MODE1 0x04
#define SPI_MODE2 0x08
#define SPI_MODE3 0x0C

#define SPI_MODE_MASK 0x0C  // CPOL = bit 3, CPHA = bit 2 on SPCR
#define SPI_CLOCK_MASK 0x03  // SPR1 = bit 1, SPR0 = bit 0 on SPCR
#define SPI_2XCLOCK_MASK 0x01  // SPI2X = bit 0 on SPSR

#define SPI_LSBFIRST 0

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)

#define SPI_PORT PORTB
#define SPI_PORT_DIR DDRB
#define SPI_BIT_MISO 0x8
#define SPI_BIT_MOSI 0x4
#define SPI_BIT_SCK 0x2
#define SPI_BIT_SS 0x1

#elif defined(__AVR_ATmega168__) || defined(__AVR_ATmega328P__)

#define SPI_PORT PORTB
#define SPI_PORT_DIR DDRB
#define SPI_BIT_SCK 0x20
#define SPI_BIT_MISO 0x10
#define SPI_BIT_MOSI 0x8
#define SPI_BIT_SS 0x4

#endif

#define SPI_MISO ((SPI_PORT & SPI_BIT_MISO) >> 3)
#define SPI_MOSI(x) { if (x) SPI_PORT |= SPI_BIT_MOSI; else SPI_PORT &= ~SPI_BIT_MOSI; }
#define SPI_SCK(x) { if (x) SPI_PORT |= SPI_BIT_SCK; else SPI_PORT &= ~SPI_BIT_SCK; }
#define SPI_SS(x) { if (x) SPI_PORT |= SPI_BIT_SS; else SPI_PORT &= ~SPI_BIT_SS; }

void spi_begin();

#endif
