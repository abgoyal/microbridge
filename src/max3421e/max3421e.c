#include "max3421e.h"
#include "../spi.h"

static uint8_t vbusState;

/*
 * Initialises the max3421e host shield. Initialises the SPI bus and sets the required pin directions.
 * Must be called before powerOn.
 */
void max3421e_init()
{
	spi_begin();

	// Set MAX_INT and MAX_GPX pins to input mode.
	MAX_PORT_DIR &= ~(MAX_BIT_INT | MAX_BIT_GPX);

	// Set SPI !SS pint to output mode.
	SPI_PORT_DIR |= MAX_BIT_SS;

	// Pull SPI !SS high
	MAX_SS(1);

	// Reset
	MAX_PORT_DIR |= MAX_BIT_RESET;
	MAX_RESET(1);
}

/**
 * Resets the max3412e. Sets the chip reset bit, SPI configuration is not affected.
 * @return true iff success.
 */
boolean max3421e_reset(void)
{
	uint8_t tmp = 0;

	//Chip reset. This stops the oscillator
	max3421e_write(USBCTL, bmCHIPRES);

	//Remove the reset
	max3421e_write(USBCTL, 0x00);

	avr_delay(10);

	// Wait until the PLL is stable
	while (!(max3421e_read(USBIRQ) & bmOSCOKIRQ))
	{
		// Timeout after 256 attempts.
		tmp++;
		if (tmp == 0)
			return (false);
	}

	// Success.
	return (true);
}

/**
 * Initialises the max3421e after power-on.
 */
void max3421e_powerOn(void)
{
	// Configure full-duplex SPI, interrupt pulse.
	max3421e_write(PINCTL, (bmFDUPSPI + bmINTLEVEL + bmGPXB)); //Full-duplex SPI, level interrupt, GPX

	// Stop/start the oscillator
	if (max3421e_reset() == false)
		avr_serialPrintf("Error: OSCOKIRQ failed to assert\n");

	// configure host operation
	max3421e_write(MODE, bmDPPULLDN | bmDMPULLDN | bmHOST | bmSEPIRQ ); // set pull-downs, Host, Separate GPIN IRQ on GPX
	max3421e_write(HIEN, bmCONDETIE | bmFRAMEIE ); //connection detection

	// Check if device is connected.
	max3421e_write(HCTL, bmSAMPLEBUS ); // sample USB bus
	while (!(max3421e_read(HCTL) & bmSAMPLEBUS))
		; //wait for sample operation to finish

	max3421e_busprobe(); //check if anything is connected
	max3421e_write(HIRQ, bmCONDETIRQ ); //clear connection detect interrupt

	//enable interrupt pin
	max3421e_write(CPUCTL, 0x01);
}

/**
 * Writes a single register.
 *
 * @param reg register address.
 * @param value value to write.
 */
void max3421e_write(uint8_t reg, uint8_t value)
{
	// Pull slave select low to indicate start of transfer.
	MAX_SS(0);

	// Transfer command byte, 0x02 indicates write.
	SPDR = (reg | 0x02);
	while (!(SPSR & (1 << SPIF)));

	// Transfer value byte.
	SPDR = value;
	while (!(SPSR & (1 << SPIF)));

	// Pull slave select high to indicate end of transfer.
	MAX_SS(1);

	return;
}

/**
 * Writes multiple bytes to a register.
 * @param reg register address.
 * @param count number of bytes to write.
 * @param vaues input values.
 * @return a pointer to values, incremented by the number of bytes written (values + length).
 */
char * max3421e_writeMultiple(uint8_t reg, uint8_t count, char * values)
{
	// Pull slave select low to indicate start of transfer.
	MAX_SS(0);

	// Transfer command byte, 0x02 indicates write.
	SPDR = (reg | 0x02);
	while (!(SPSR & (1 << SPIF)));

	// Transfer values.
	while (count--)
	{
		// Send next value byte.
		SPDR = (*values);
		while (!(SPSR & (1 << SPIF)));

		values++;
	}

	// Pull slave select high to indicate end of transfer.
	MAX_SS(1);

	return (values);
}

/**
 * Reads a single register.
 *
 * @param reg register address.
 * @return result value.
 */
uint8_t max3421e_read(uint8_t reg)
{
	// Pull slave-select high to initiate transfer.
	MAX_SS(0);

	// Send a command byte containing the register number.
	SPDR = reg;
	while (!(SPSR & (1 << SPIF)));

	// Send an empty byte while reading.
	SPDR = 0;
	while (!(SPSR & (1 << SPIF)));

	// Pull slave-select low to signal transfer complete.
	MAX_SS(1);

	// Return result byte.
	return (SPDR);
}

/**
 * Reads multiple bytes from a register.
 *
 * @param reg register to read from.
 * @param count number of bytes to read.
 * @param values target buffer.
 * @return pointer to the input buffer + count.
 */
char * max3421e_readMultiple(uint8_t reg, uint8_t count, char * values)
{
	// Pull slave-select high to initiate transfer.
	MAX_SS(0);

	// Send a command byte containing the register number.
	SPDR = reg;
	while (!(SPSR & (1 << SPIF))); //wait

	// Read [count] bytes.
	while (count--)
	{
		// Send empty byte while reading.
		SPDR = 0;
		while (!(SPSR & (1 << SPIF)));

		*values = SPDR;
		values++;
	}

	// Pull slave-select low to signal transfer complete.
	MAX_SS(1);

	// Return the byte array + count.
	return (values);
}

/**
 * @return the status of Vbus.
 */
uint8_t max3421e_getVbusState()
{
	return vbusState;
}

/**
 * Probes the bus to determine device presence and speed, and switches host to this speed.
 */
void max3421e_busprobe(void)
{
	uint8_t bus_sample;
	bus_sample = max3421e_read(HRSL); //Get J,K status
	bus_sample &= (bmJSTATUS | bmKSTATUS); //zero the rest of the uint8_t

	switch (bus_sample)
	{ //start full-speed or low-speed host
	case (bmJSTATUS):
		if ((max3421e_read(MODE) & bmLOWSPEED) == 0)
		{
			max3421e_write(MODE, MODE_FS_HOST ); //start full-speed host
			vbusState = FSHOST;
		} else
		{
			max3421e_write(MODE, MODE_LS_HOST); //start low-speed host
			vbusState = LSHOST;
		}
		break;
	case (bmKSTATUS):
		if ((max3421e_read(MODE) & bmLOWSPEED) == 0)
		{
			max3421e_write(MODE, MODE_LS_HOST ); //start low-speed host
			vbusState = LSHOST;
		} else
		{
			max3421e_write(MODE, MODE_FS_HOST ); //start full-speed host
			vbusState = FSHOST;
		}
		break;
	case (bmSE1): //illegal state
		vbusState = SE1;
		break;
	case (bmSE0): //disconnected state
		vbusState = SE0;
		break;
	}
}

/**
 * MAX3421 state change task and interrupt handler.
 * @return error code or 0 if successful.
 */
uint8_t max3421e_poll(void)
{
	uint8_t rcode = 0;

	// Check interrupt.
	if (MAX_INT() == 0)
		rcode = max3421e_IntHandler();

	if (MAX_GPX() == 0)
		max3421e_GpxHandler();

	return (rcode);
}

/**
 * Interrupt handler.
 */
uint8_t max3421e_IntHandler(void)
{
	uint8_t interruptStatus;
	uint8_t HIRQ_sendback = 0x00;

	// Determine interrupt source.
	interruptStatus = max3421e_read(HIRQ);

	if (interruptStatus & bmFRAMEIRQ)
	{
		//->1ms SOF interrupt handler
		HIRQ_sendback |= bmFRAMEIRQ;
	}

	if (interruptStatus & bmCONDETIRQ)
	{
		max3421e_busprobe();

		HIRQ_sendback |= bmCONDETIRQ;
	}

	// End HIRQ interrupts handling, clear serviced IRQs
	max3421e_write(HIRQ, HIRQ_sendback);

	return (HIRQ_sendback);
}

/**
 * GPX interrupt handler
 */
uint8_t max3421e_GpxHandler(void)
{
	//read GPIN IRQ register
	uint8_t interruptStatus = max3421e_read(GPINIRQ);

	//    if( GPINIRQ & bmGPINIRQ7 ) {            //vbus overload
	//        vbusPwr( OFF );                     //attempt powercycle
	//        delay( 1000 );
	//        vbusPwr( ON );
	//        regWr( rGPINIRQ, bmGPINIRQ7 );
	//    }

	return (interruptStatus);
}

