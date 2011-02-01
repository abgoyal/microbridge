
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

	return usb_bulkWrite(&(handle->device), sizeof(adb_message), &message);
}

int adb_writeMessage(adb_usbHandle * handle, uint32_t command, uint32_t arg0, uint32_t arg1, uint32_t length, uint8_t * data)
{
	adb_message message;
	uint32_t count, sum = 0;
	uint8_t * x;
	uint16_t totalLength = sizeof(adb_message) + length;
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

	rcode = usb_bulkWrite(&(handle->device), sizeof(adb_message), &message);
	if (rcode) return rcode;

	rcode = usb_bulkWrite(&(handle->device), length, data);

	return rcode;

}

int adb_writeString(adb_usbHandle * handle, uint32_t command, uint32_t arg0, uint32_t arg1, char * str)
{
	return adb_writeMessage(handle, command, arg0, arg1, strlen(str) + 1, str);
}

uint8_t adb_buf[1024];

int adb_poll(adb_usbHandle * handle)
{
	uint8_t buf[ADB_USB_PACKETSIZE], rcode;
	unsigned int bytesRead;
	adb_message message;

	// Poll a packet from the USB
	bytesRead = usb_bulkRead(&(handle->device), ADB_USB_PACKETSIZE, &message);

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
		bytesRead = usb_bulkRead(&(handle->device), len, bufPtr);
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
 * @param address USB device address.
 * @param configuration device configuration.
 * @param handle pointer to an adb_usbHandle struct. The endpoint device address, configuration, and endpoint information will be stored here.
 * @return true iff the device is an ADB device.
 */
boolean adb_isAdbDevice(uint8_t address, uint8_t configuration, adb_usbHandle * handle)
{
	boolean ret = false;
	uint8_t err = 0;
	uint8_t buf[MAX_BUF];

	usb_bulkDevice device;
	device.address = address;
	device.control.address = 0;
	device.control.sendToggle = bmSNDTOG0;
	device.control.receiveToggle = bmRCVTOG0;

	avr_serialPrintf("checking address=%d\n", address);

	// Read the length of the configuration descriptor.
	err = usb_getConfigurationDescriptor(&device, 4, configuration, buf, USB_NAK_LIMIT);
	if (err) return false;

	// Clamp to the length to MAX_BUF.
	uint16_t length = (buf[3] << 8) | buf[2];
	if (length>MAX_BUF) length = MAX_BUF;

	// Read the full configuration descriptor.
	err = usb_getConfigurationDescriptor(&device, length, configuration, buf, USB_NAK_LIMIT);
	if (err) return false;

	uint16_t pos = 0;
	uint8_t descriptorLength;
	uint8_t descriptorType;

	usb_configurationDescriptor * config;
	usb_interfaceDescriptor * interface;
	usb_endpointDescriptor * endpoint;

	while (pos < length)
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
				handle->address = address;
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
	// Copy the control endpoint directly.
	ep_record[0] = *(usb_getDevTableEntry(0, 0));

	// Output endpoint.
    ep_record[1].address = handle->outputEndPointAddress;
    ep_record[1].attributes = USB_TRANSFER_TYPE_BULK;
    ep_record[1].maxPacketSize = ADB_USB_PACKETSIZE;
    ep_record[1].sendToggle = bmSNDTOG0;
    ep_record[1].receiveToggle = bmRCVTOG0;

    // Input endpoint.
    ep_record[2].address = handle->inputEndPointAddress;
    ep_record[2].attributes = USB_TRANSFER_TYPE_BULK;
    ep_record[2].maxPacketSize = ADB_USB_PACKETSIZE;
    ep_record[2].sendToggle = bmSNDTOG0;
    ep_record[2].receiveToggle = bmRCVTOG0;

    // Copy endpoint structs to the adb handle record.
    handle->controlEndPoint = &(ep_record[0]);
    handle->outputEndPoint = &(ep_record[1]);
    handle->inputEndPoint = &(ep_record[2]);

    usb_setDevTableEntry(handle->address, ep_record);

    // Configure bulk device
    handle->device.address = handle->address;
    handle->device.control = *(handle->controlEndPoint);
    handle->device.in = *(handle->inputEndPoint);
    handle->device.out = *(handle->outputEndPoint);

    uint8_t rcode;

    avr_serialPrintf("Configuring  ... \n");

    // Configure the USB lib to use the device address, configuration, and endpoint settings specified in the handle.
    // rcode = usb_setConfiguration(handle->address, handle->controlEndPoint->address, handle->configuration, USB_NAK_LIMIT);
    // if (rcode) return rcode;
    rcode = usb_setConfiguration(&(handle->device), handle->configuration, USB_NAK_LIMIT);
    if (rcode) return rcode;

    rcode = usb_setConfiguration(&(handle->device), handle->configuration, USB_NAK_LIMIT);

    return rcode;
}

/**
 * Prints info about a connected device to the serial port.
 * @param address device address.
 * @return 0 if successful, standard MAX3421e error code otherwise.
 */
int adb_printDeviceInfo(uint8_t address)
{
	/*
	int rcode;
    char buf[128];

    // Read the device descriptor
	usb_deviceDescriptor deviceDescriptor;
    rcode = usb_getDeviceDescriptor(address, 0x0 , sizeof(deviceDescriptor) , &deviceDescriptor, USB_NAK_LIMIT);
    if (rcode) return rcode;

    avr_serialPrintf("Vendor ID: %x\n", deviceDescriptor.idVendor);
    avr_serialPrintf("Product ID: %x\n", deviceDescriptor.idProduct);
    avr_serialPrintf("Configuration count: %d\n", deviceDescriptor.bNumConfigurations);
    avr_serialPrintf("Device class: %d\n", deviceDescriptor.bDeviceClass);
    avr_serialPrintf("Device subclass: %d\n", deviceDescriptor.bDeviceSubClass);
    avr_serialPrintf("Device protocol: %d\n", deviceDescriptor.bDeviceProtocol);

    usb_getString(address, deviceDescriptor.iManufacturer, 128, buf);
    avr_serialPrintf("Manufacturer: %s\n", buf);
    usb_getString(address, deviceDescriptor.iProduct, 128, buf);
    avr_serialPrintf("Product: %s\n", buf);
    usb_getString(address, deviceDescriptor.iSerialNumber, 128, buf);
    avr_serialPrintf("Serial number: %s\n", buf);
    */

    return 0;
}

