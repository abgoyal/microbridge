#include "avr.h"
#include "spi.h"
#include "adb.h"
#include "max3421e/max3421e_usb.h"

int main()
{
	// Initialise avr timers
	avr_timerInit();

	// Initialise serial port
	avr_serialInit(57600);

 	// Initialise USB host shield.
 	max3421e_init();
	max3421e_powerOn();
	usb_init();

 	adb_usbHandle handle;

 	boolean connected = false;

 	// HACK, init servo control
 	TCCR3A = 0;

 	TCCR3A = (1<<COM3B1) | (1<<COM3C1) | (1<<WGM31);
 	TCCR3B = (1<<WGM33) | (1<<WGM32) | (1<<CS31);
 	ICR3 = 40000;

 	DDRE |= (1<<4) | (1<<5);

 	int c = 3000;

 	//for (c=1000; c<5000; c+=10)
 	{
		OCR3B = c;
		OCR3C = c;
 		avr_delay(10);
 	}

	avr_serialPrint("Starting ... \n");

	while (1)
 	{
 		max3421e_poll();
 		usb_poll();

 		if (usb_getUsbTaskState() == USB_STATE_CONFIGURING)
		{

 			// avr_serialPrint("Checking for ADB device ... \n");

 			usb_device * device = usb_getDevice(1);

 			if (adb_isAdbDevice(device, 0, &handle))
 			{
 				avr_serialPrint("ADB device found!\n");
 				adb_initUsb(&handle);
 			}

 			usb_setUsbTaskState(USB_STATE_RUNNING);

		}

 		if (usb_getUsbTaskState() == USB_STATE_RUNNING)
 		{
 			if (!connected)
 			{
 	 			uint8_t rcode;

 	 			avr_serialPrintf("Sending CONNECT message ...\n");

 	 			rcode = adb_writeString(&handle, A_CNXN, 0x01000000, 4096, "host::microbridge");
 	 			avr_serialPrintf("connect: %d\n", rcode);

 	 			connected = true;
 			}

 			adb_poll(&handle);

 		}

 	}

	return 0;
}
