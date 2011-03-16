CC=avr-gcc

#MCU=atmega168
MCU=atmega328p
#MCU=atmega1280
CFLAGS=-mmcu=${MCU} -Wall -Os -pedantic -std=c99 #-DSFHACK
LINKERFLAGS=-mmcu=${MCU}

MAIN=main
CFILES=${MAIN}.c avr.c spi.c usb.c max3421e/max3421e.c max3421e/max3421e_usb.c adb.c
OFILES=${CFILES:.c=.o}
TARGET=microbridge

all: ${TARGET}.elf
	avr-objcopy -I elf32-avr -O ihex ${TARGET}.elf ${TARGET}.ihex
	avr-objcopy -I elf32-avr -O binary ${TARGET}.elf ${TARGET}.bin
	
${TARGET}.elf: ${OFILES}
	${CC} ${LINKERFLAGS} ${OFILES} -o ${TARGET}.elf
	
%.o: %.c
	${CC} ${CFLAGS} -c $< -o ${<:.c=.o}

clean:
	rm -f `find -name \*.o`
	rm -f ${TARGET}.elf
	rm -f ${TARGET}.bin
	rm -f ${TARGET}.ihex
	rm -f ${TARGET}
