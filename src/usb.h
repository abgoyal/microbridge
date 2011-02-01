/*
 *
 */

#ifndef __usb__
#define __usb_

#include <stdint.h>
#include <stdbool.h>

/**
 * USB endpoint.
 */
typedef struct
{
    // Endpoint address. Bit 7 indicates direction (out=0, in=1).
	uint8_t address;

	// Endpoint transfer type.
	uint8_t attributes;

	// Maximum packet size.
    uint16_t maxPacketSize;

    // The max3421e uses these bits to toggle between DATA0 and DATA1.
    uint8_t sendToggle;
    uint8_t receiveToggle;

} usb_endpoint;

/**
 * USB device.
 */
typedef struct
{
	// Device address.
	uint8_t address;

	// Bulk endpoints
	usb_endpoint control, in, out;

} usb_bulkDevice;

void usb_init();
void usb_poll();
void usb_endpoint_create();

int usb_bulkRead(usb_bulkDevice * device, unsigned int length, char * data);
int usb_bulkWrite(usb_bulkDevice * device, unsigned int length, char * data);

#endif
