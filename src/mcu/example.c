#include "avr.h"
#include "adb.h"

// Event handler to process incoming data from ADB.
void adbEventHandler(adb_connection * connection, adb_eventType event, uint16_t length, uint8_t * data)
{
	int i;

	switch (event)
	{
	case ADB_CONNECTION_RECEIVE:

		// Write the results to UART
		for (i=0; i<length; i++)
			avr_serialPrintf("%c", data[i]);

		break;
	default:
		break;
	}

}

int main()
{
	// Initialise avr timers
	avr_timerInit();

	// Initialise serial port
	avr_serialInit(57600);

 	// Initialise ADB connection
	adb_init();

	// Create a new ADB connection, run logcat
	adb_addConnection("shell:exec logcat -s MYAPP:*", false, adbEventHandler);

	// ADB polling.
	while (1)
		adb_poll();

	return 0;
}
