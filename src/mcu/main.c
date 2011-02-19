#include "avr.h"
#include "adb.h"

static uint8_t c;

adb_connection * shell;

ISR(USART0_RX_vect)
{
	if (c == 0)
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

	uint32_t lastTime = avr_millis();

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
