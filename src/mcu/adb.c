#include <string.h>

#include "adb.h"

// Three endpoints for control, output, and input respectively.
static usb_endpoint ep_record[3];

#define ADB_CLASS 0xff
#define ADB_SUBCLASS 0x42
#define ADB_PROTOCOL 0x1
#define MAX_BUF 256

// static uint8_t adb_transferBuf[256];

int adb_writeEmptyMessage(adb_usbHandle * handle, uint32_t command, uint32_t arg0, uint32_t arg1)
{
	adb_message message;

	message.command = command;
	message.arg0 = arg0;
	message.arg1 = arg1;
	message.data_length = 0;
	message.data_check = 0;
	message.magic = command ^ 0xffffffff;

	return usb_bulkWrite(handle->device, sizeof(adb_message), (uint8_t*)&message);
}

int adb_writeMessage(adb_usbHandle * handle, uint32_t command, uint32_t arg0, uint32_t arg1, uint32_t length, uint8_t * data)
{
	adb_message message;
	uint32_t count, sum = 0;
	uint8_t * x;
	uint8_t rcode;

	// Calculate data checksum
    count = length;
    x = data;
    while(count-- > 0) sum += *x++;

	// Fill out the message record.
	message.command = command;
	message.arg0 = arg0;
	message.arg1 = arg1;
	message.data_length = length;
	message.data_check = sum;
	message.magic = command ^ 0xffffffff;

	rcode = usb_bulkWrite(handle->device, sizeof(adb_message), (uint8_t*)&message);
	if (rcode) return rcode;

	rcode = usb_bulkWrite(handle->device, length, data);

	return rcode;

}

int adb_writeString(adb_usbHandle * handle, uint32_t command, uint32_t arg0, uint32_t arg1, char * str)
{
	return adb_writeMessage(handle, command, arg0, arg1, strlen(str) + 1, (uint8_t*)str);
}

uint8_t adb_buf[1024];

int adb_poll(adb_usbHandle * handle)
{
	unsigned int bytesRead;
	adb_message message;

	// Poll a packet from the USB
	bytesRead = usb_bulkRead(handle->device, ADB_USB_PACKETSIZE, (uint8_t*)&message);

	// Check if the USB in transfer was successful.
	if (bytesRead<0) return -1;

	// Check if the received number of bytes matches our expected 24 bytes of ADB message header.
	if (bytesRead != sizeof(adb_message)) return -2;

	uint32_t bytesLeft = message.data_length;
	uint8_t * bufPtr = adb_buf;
	while (bytesLeft>0)
	{
		int len = bytesLeft < ADB_USB_PACKETSIZE ? bytesLeft : ADB_USB_PACKETSIZE;

		// Read payload
		bytesRead = usb_bulkRead(handle->device, len, bufPtr);
		if (bytesRead < 0) return -3;

		bytesLeft -= bytesRead;
		bufPtr += bytesRead;

	}
	bufPtr[0] = 0;

	int value, i;

	// Handle the different commands.
	switch (message.command)
	{
	case A_CNXN:
		avr_serialPrintf("ADB CONNECTION command\n");

		avr_serialPrintf("\t%s\n", adb_buf);

		// ad_writeEmptyMessage(handle, A_OKAY, 1, 0);
		adb_writeString(handle, A_OPEN, 1, 0, "tcp:4567");

		break;
	case A_OKAY:
		avr_serialPrintf("ADB OKAY command\n");
		break;
	case A_OPEN:
		avr_serialPrintf("ADB OPEN command\n");
		break;
	case A_CLSE:
		avr_serialPrintf("ADB CLOSE command\n");
		avr_serialPrintf("\t local-id:%d remote-id:%d closed\n", message.arg0, message.arg1);
		break;
	case A_SYNC:
		avr_serialPrintf("ADB SYNC command\n");
		break;
	case A_WRTE:
		// avr_serialPrintf("ADB WRITE command, local-id: %ld, remote-id:%d\n", message.arg0, message.arg1);

		for (i=0; i<message.data_length; i++)
			avr_serialPrintf("%c", adb_buf[i]);

		value = (adb_buf[1]-'0')*10 + (adb_buf[2]-'0');

		OCR3C = 3000 + (value-50) * 30;
		OCR3B = 3000 + (value-50) * 30;

		adb_writeEmptyMessage(handle, A_OKAY, message.arg1, message.arg0);

		break;
	default:
		avr_serialPrintf("Unknown ADB command: %lx\n", message.command);
		break;
	}

	return 0;

}

/**
 * Helper function for usb_isAdbDevice to check whether an interface is a valid ADB interface.
 * @param interface interface descriptor struct.
 */
static boolean usb_isAdbInterface(usb_interfaceDescriptor * interface)
{

	// Check if the interface has exactly two endpoints.
	if (interface->bNumEndpoints!=2) return false;

	// Check if the endpoint supports bulk transfer.
	if (interface->bInterfaceProtocol != ADB_PROTOCOL) return false;
	if (interface->bInterfaceClass != ADB_CLASS) return false;
	if (interface->bInterfaceSubClass != ADB_SUBCLASS) return false;

	return true;
}

/**
 * Checks whether the a connected USB device is an ADB device and populates a handle if it is.
 *
 * @param device USB device.
 * @param handle pointer to an adb_usbHandle struct. The endpoint device address, configuration, and endpoint information will be stored here.
 * @return true iff the device is an ADB device.
 */
boolean adb_isAdbDevice(usb_device * device, int configuration, adb_usbHandle * handle)
{
	boolean ret = false;
	uint8_t err = 0;
	uint8_t buf[MAX_BUF];
	int bytesRead;

	handle->device = device;

	// Read the length of the configuration descriptor.
	bytesRead = usb_getConfigurationDescriptor(device, configuration, MAX_BUF, buf);
	if (bytesRead<0) return false;

	uint16_t pos = 0;
	uint8_t descriptorLength;
	uint8_t descriptorType;

	usb_configurationDescriptor * config;
	usb_interfaceDescriptor * interface;
	usb_endpointDescriptor * endpoint;

	while (pos < bytesRead)
	{
		descriptorLength = buf[pos];
		descriptorType = buf[pos + 1];

		switch (descriptorType)
		{
		case (USB_DESCRIPTOR_CONFIGURATION):
			config = (usb_configurationDescriptor *)(buf + pos);
//			avr_serialPrintf("CONFIG: %d\n", config->bConfigurationValue);
			break;
		case (USB_DESCRIPTOR_INTERFACE):
			interface = (usb_interfaceDescriptor *)(buf + pos);
//			avr_serialPrintf("INTERFACE class=%d subclass=%d protocol=%d enpoints=%d, adb=%s\n", interface->bInterfaceClass, interface->bInterfaceSubClass, interface->bInterfaceProtocol, interface->bNumEndpoints, usb_isAdbInterface(interface) ? "true" : "false");
//			avr_serialPrintf("\talternate=%d\n", interface->bAlternateSetting);

			if (usb_isAdbInterface(interface))
			{
				// handle->address = address;
				handle->configuration = config->bConfigurationValue;
				handle->interface = interface->bInterfaceNumber;

				// Detected ADB interface!
				ret = true;
			}
			break;
		case (USB_DESCRIPTOR_ENDPOINT):
			endpoint = (usb_endpointDescriptor *)(buf + pos);
//			avr_serialPrintf("ENDPOINT %d (%s %d)\n", endpoint->bEndpointAddress, endpoint->bEndpointAddress & 0x80 ? "in" : "out", endpoint->bEndpointAddress & 0x7f);
//			avr_serialPrintf("\t max packet size=0x%02x, attributesibutes=0x%02x\n", endpoint->wMaxPacketSize, endpoint->bmattributesibutes);

			// If this endpoint descriptor is found right after the ADB interface descriptor, it belong to that interface.
			if (interface->bInterfaceNumber == handle->interface)
				if (endpoint->bEndpointAddress & 0x80)
					handle->inputEndPointAddress = endpoint->bEndpointAddress & ~0x80;
				else
					handle->outputEndPointAddress = endpoint->bEndpointAddress;

			break;
		default:
			break;
		}

		pos += descriptorLength;
	}

	return ret;

}

int adb_initUsb(adb_usbHandle * handle)
{
	usb_initDevice(handle->device, handle->configuration);

	usb_initEndPoint(&handle->device->bulk_in, handle->inputEndPointAddress);
	handle->device->bulk_in.attributes = USB_TRANSFER_TYPE_BULK;
	handle->device->bulk_in.maxPacketSize = ADB_USB_PACKETSIZE;

	usb_initEndPoint(&handle->device->bulk_out, handle->outputEndPointAddress);
	handle->device->bulk_out.attributes = USB_TRANSFER_TYPE_BULK;
	handle->device->bulk_out.maxPacketSize = ADB_USB_PACKETSIZE;

	return 0;
}

