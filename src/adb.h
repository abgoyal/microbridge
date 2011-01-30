#ifndef __adb_h__
#define __adb_h__

#include <stdint.h>
#include <stdbool.h>

#include "max3421e/max3421e_usb.h"

// ADB
#define MAX_PAYLOAD 4096;
#define A_SYNC 0x434e5953
#define A_CNXN 0x4e584e43
#define A_OPEN 0x4e45504f
#define A_OKAY 0x59414b4f
#define A_CLSE 0x45534c43
#define A_WRTE 0x45545257

#define ADB_USB_PACKETSIZE 0x40

typedef struct
{
	uint8_t address;
	uint8_t configuration;
	uint8_t interface;
	uint8_t inputEndPointAddress;
	uint8_t outputEndPointAddress;

	usb_endpoint * controlEndPoint;
	usb_endpoint * outputEndPoint;
	usb_endpoint * inputEndPoint;
} adb_usbHandle;

typedef struct
{
	uint32_t command;       /* command identifier constant      */
	uint32_t arg0;          /* first argument                   */
	uint32_t arg1;          /* second argument                  */
	uint32_t data_length;   /* length of payload (0 is allowed) */
	uint32_t data_check;    /* checksum of data payload         */
	uint32_t magic;         /* command ^ 0xffffffff             */
} adb_message;

boolean adb_isAdbDevice(uint8_t address, uint8_t configuration, adb_usbHandle * handle);
int adb_initUsb(adb_usbHandle * handle);
int adb_printDeviceInfo(uint8_t address);
int adb_writeString(adb_usbHandle * handle, uint32_t command, uint32_t arg0, uint32_t arg1, char * str);
int adb_poll(adb_usbHandle * handle);

#endif
