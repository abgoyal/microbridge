#include "avr.h"
#include "adb.h"

static uint8_t c = 0;

adb_connection * shell;

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
ISR(USART0_RX_vect)
#elif defined(__AVR_ATmega328P__)
ISR(USART_RX_vect)
#endif
{
	c = UDR0;
}

void adbEventHandler(adb_connection * connection, adb_eventType event, uint16_t length, uint8_t * data)
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

		for (i=0; i<length; i++)
			avr_serialPrintf("%c", data[i]);

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
	shell = adb_addConnection("shell:", true, adbEventHandler);

	while (1)
 	{
		adb_poll();

 		if (c!=0 && shell->status == ADB_OPEN)
 		{
 			adb_write(shell, 1, &c);
 			c = 0;
 		}

 	}

	return 0;
}
