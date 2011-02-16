#include "avr.h"
#include "spi.h"
#include "adb.h"
#include "max3421e/max3421e_usb.h"

#include <avr/io.h>
#include <avr/interrupt.h>

static adb_connection * shell;
static char c;

ISR(USART0_RX_vect)
{
	c = UDR0;
}

void adbEventHandler(adb_connection * connection, adb_eventType event, uint16_t length, char * data)
{
	int i;

	switch (event)
	{
	case ADB_CONNECT:
		avr_serialPrintf("ADB EVENT CONNECT\n");
		break;
	case ADB_DISCONNECT:
		avr_serialPrintf("ADB EVENT DISCONNECT\n");
		break;
	case ADB_CONNECTION_OPEN:
		avr_serialPrintf("ADB EVENT OPEN connection=[%s]\n", connection->connectionString);
		break;
	case ADB_CONNECTION_CLOSE:
		avr_serialPrintf("ADB EVENT CLOSE connection=[%s]\n", connection->connectionString);
		break;
	case ADB_CONNECTION_FAILED:
		avr_serialPrintf("ADB EVENT FAILED connection=[%s]\n", connection->connectionString);
		break;
	case ADB_CONNECTION_RECEIVE:
//		avr_serialPrintf("ADB EVENT RECEIVE connection=[%s]\n", connection->connectionString);

		for (i=0; i<length; i++)
			avr_serialPrintf("%c", data[i]);

//		avr_serialPrintf("\n");

		break;
	}

}

int main()
{
	// Initialise avr timers
	avr_timerInit();

	// Initialise serial port
	avr_serialInit(57600);

 	// Initialise USB host shield.
	adb_init();
	adb_setEventHandler(adbEventHandler);

	// adb_addConnection("tcp:4567", true);
	shell = adb_addConnection("shell:", true);

	while (1)
 	{
		adb_poll();

 		if (c!=0)
 		{
 			if (shell->status != ADB_OPEN)
 				avr_serialPrintf("Connection NOT open %d\n", shell->status);

 			adb_write(shell, 1, &c);

 			c = 0;
 		}

 	}

	return 0;
}
