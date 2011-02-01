/* USB functions */
#include "../avr.h"
#include "max3421e_usb.h"
#include "../ch9.h"

static uint8_t usb_error = 0;
static uint8_t usb_task_state = USB_DETACHED_SUBSTATE_INITIALIZE;

usb_deviceRecord devtable[USB_NUMDEVICES + 1];

usb_bulkDevice deviceZero;

/**
 * Initialises the USB layer.
 */
void usb_init()
{
	uint8_t i;

	// Initialise the USB state machine.
	usb_task_state = USB_DETACHED_SUBSTATE_INITIALIZE;

	// Initialise the device table.
	for (i = 0; i < (USB_NUMDEVICES + 1); i++)
	{
		devtable[i].epinfo = NULL;
		devtable[i].devclass = 0;
	}

	deviceZero.address = 0;
	deviceZero.control.address = 0;
	deviceZero.control.sendToggle = bmSNDTOG0;
	deviceZero.control.receiveToggle = bmRCVTOG0;
}

uint8_t usb_getUsbTaskState()
{
	return (usb_task_state);
}

void usb_setUsbTaskState(uint8_t state)
{
	usb_task_state = state;
}

usb_endpoint * usb_getDevTableEntry(uint8_t addr, uint8_t ep)
{
	usb_endpoint * ptr;
	ptr = devtable[addr].epinfo;
	ptr += ep;
	return (ptr);
}

/* set device table entry */
/* each device is different and has different number of endpoints. This function plugs endpoint record structure, defined in application, to devtable */
void usb_setDevTableEntry(uint8_t addr, usb_endpoint * eprecord_ptr)
{
	devtable[addr].epinfo = eprecord_ptr;
}


int usb_dispatchPacket2(uint8_t token, usb_endpoint * endpoint)
{
	unsigned long timeout = avr_millis() + USB_XFER_TIMEOUT;
	uint8_t tmpdata;
	uint8_t rcode = 0;
	unsigned int nak_count = 0;
	char retry_count = 0;

	avr_serialPrintf("usb_dispatchPacket %d\n", endpoint->address);

	while (timeout > avr_millis())
	{
		max3421e_write(MAX_REG_HXFR, (token | endpoint->address)); //launch the transfer
		rcode = 0xff;
		while (avr_millis() < timeout)
		{ //wait for transfer completion
			tmpdata = max3421e_read(MAX_REG_HIRQ);
			if (tmpdata & bmHXFRDNIRQ)
			{
				max3421e_write(MAX_REG_HIRQ, bmHXFRDNIRQ); //clear the interrupt
				rcode = 0x00;
				break;
			}//if( tmpdata & bmHXFRDNIRQ
		}//while ( avr_millis() < timeout

		if (rcode != 0x00)
		{ //exit if timeout
			return (rcode);
		}

		rcode = (max3421e_read(MAX_REG_HRSL) & 0x0f); //analyze transfer result

		switch (rcode)
		{
		case hrNAK:
			nak_count++;
			if (nak_count == USB_NAK_LIMIT)
			{
				return (rcode);
			}
			break;
		case hrTIMEOUT:
			retry_count++;
			if (retry_count == USB_RETRY_LIMIT)
			{
				return (rcode);
			}
			break;
		default:
			return (rcode);
		}//switch( rcode
	}//while( timeout > avr_millis()

	avr_serialPrintf("RCODE=%d\n", rcode);

	return (rcode);
}

/**
 * USB main task. Performs enumeration/cleanup
 */
void usb_poll(void) //USB state machine
{
	uint8_t i;
	uint8_t rcode;
	static uint8_t tmpaddr;
	uint8_t tmpdata;
	static unsigned long delay = 0;
	usb_deviceDescriptor buf;
	tmpdata = max3421e_getVbusState();
	/* modify USB task state if Vbus changed */

	switch (tmpdata)
	{
	case SE1: //illegal state
		usb_task_state = USB_DETACHED_SUBSTATE_ILLEGAL;
		break;
	case SE0: //disconnected
		if ((usb_task_state & USB_STATE_MASK) != USB_STATE_DETACHED)
		{
			usb_task_state = USB_DETACHED_SUBSTATE_INITIALIZE;
		}
		break;
	case FSHOST: //attached
	case LSHOST:
		if ((usb_task_state & USB_STATE_MASK) == USB_STATE_DETACHED)
		{
			delay = avr_millis() + USB_SETTLE_DELAY;
			usb_task_state = USB_ATTACHED_SUBSTATE_SETTLE;
		}
		break;
	}// switch( tmpdata
	//Serial.print("USB task state: ");
	//Serial.println( usb_task_state, HEX );
	switch (usb_task_state)
	{
	case USB_DETACHED_SUBSTATE_INITIALIZE:
		usb_init();
		usb_task_state = USB_DETACHED_SUBSTATE_WAIT_FOR_DEVICE;
		break;
	case USB_DETACHED_SUBSTATE_WAIT_FOR_DEVICE: //just sit here
		break;
	case USB_DETACHED_SUBSTATE_ILLEGAL: //just sit here
		break;
	case USB_ATTACHED_SUBSTATE_SETTLE: //setlle time for just attached device
		if (delay < avr_millis())
		{
			usb_task_state = USB_ATTACHED_SUBSTATE_RESET_DEVICE;
		}
		break;
	case USB_ATTACHED_SUBSTATE_RESET_DEVICE:
		max3421e_write(MAX_REG_HCTL, bmBUSRST); //issue bus reset
		usb_task_state = USB_ATTACHED_SUBSTATE_WAIT_RESET_COMPLETE;
		break;
	case USB_ATTACHED_SUBSTATE_WAIT_RESET_COMPLETE:
		if ((max3421e_read(MAX_REG_HCTL) & bmBUSRST) == 0)
		{
			tmpdata = max3421e_read(MAX_REG_MODE) | bmSOFKAENAB; //start SOF generation
			max3421e_write(MAX_REG_MODE, tmpdata);
			//                  max3421e_regWr( rMODE, bmSOFKAENAB );
			usb_task_state = USB_ATTACHED_SUBSTATE_WAIT_SOF;
			delay = avr_millis() + 20; //20ms wait after reset per USB spec
		}
		break;
	case USB_ATTACHED_SUBSTATE_WAIT_SOF: //todo: change check order
		if (max3421e_read(MAX_REG_HIRQ) & bmFRAMEIRQ)
		{ //when first SOF received we can continue
			if (delay < avr_millis())
			{ //20ms passed
				usb_task_state
						= USB_ATTACHED_SUBSTATE_GET_DEVICE_DESCRIPTOR_SIZE;
			}
		}
		break;
	case USB_ATTACHED_SUBSTATE_GET_DEVICE_DESCRIPTOR_SIZE:
		// toggle( BPNT_0 );
		// devtable[0].epinfo->maxPacketSize = 8; //set max.packet size to min.allowed

		deviceZero.control.maxPacketSize = 8;

		avr_serialPrintf("getDeviceDescriptor %d %d\n", deviceZero.address, deviceZero.control.address);
		rcode = usb_getDeviceDescriptor(&deviceZero, 8, (char *) &buf);
		avr_serialPrintf("/getDeviceDescriptor\n");
		if (rcode == 0)
		{
			deviceZero.control.maxPacketSize = buf.bMaxPacketSize0;
			usb_task_state = USB_STATE_ADDRESSING;
		} else
		{
			usb_error = USB_ATTACHED_SUBSTATE_GET_DEVICE_DESCRIPTOR_SIZE;
			usb_task_state = USB_STATE_ERROR;
		}
		break;
	case USB_STATE_ADDRESSING:
		for (i = 1; i < USB_NUMDEVICES; i++)
		{
			if (devtable[i].epinfo == NULL)
			{
				devtable[i].epinfo = devtable[0].epinfo; //set correct MaxPktSize
				//temporary record
				//until plugged with real device endpoint structure
				rcode = usb_setAddress(&deviceZero, i, USB_NAK_LIMIT);
				if (rcode == 0)
				{
					tmpaddr = i;
					usb_task_state = USB_STATE_CONFIGURING;
				} else
				{
					usb_error = USB_STATE_ADDRESSING; //set address error
					usb_task_state = USB_STATE_ERROR;
				}
				break; //break if address assigned or error occured during address assignment attempt
			}
		}//for( i = 1; i < USB_NUMDEVICES; i++
		if (usb_task_state == USB_STATE_ADDRESSING)
		{ //no vacant place in devtable
			usb_error = 0xfe;
			usb_task_state = USB_STATE_ERROR;
		}
		break;
	case USB_STATE_CONFIGURING:
		break;
	case USB_STATE_RUNNING:
		break;
	case USB_STATE_ERROR:
		break;
	}// switch( usb_task_state
}

/**
 * Convenience method for getting a string. This is useful for printing manufacturer names, serial numbers, etc.
 * Language is defaulted to zero. Strings are unicode, so a naive conversion is done where every second byte is
 * ignored.
 *
 * @param address device address.
 * @param index string index.
 */
int usb_getString(uint8_t address, uint8_t index, unsigned int length, char * str)
{
	/*
	// Buffer size hardcoded to 128
	char buf[128];
	uint8_t stringLength = 0;
	int i, ret;

	// avr_serialPrintf("getString %d %d %d\n", address, index, length);

	// Get the first byte of the string descriptor, which contains the length.
	ret = usb_getStringDescriptor(address, 0x0 , sizeof(uint8_t), index, 0x0 , (char*)&stringLength, USB_NAK_LIMIT);
	if (ret) return ret;
	if (stringLength>128) stringLength = 128;

	// avr_serialPrintf("getDescriptor\n");

	// Get the whole thing.
	ret = usb_getStringDescriptor(address, 0x0, stringLength, index, 0x0, buf, USB_NAK_LIMIT);
	if (ret) return ret;
	stringLength = (stringLength - 2) / 2;

	// Don't write outside the bounds of the target buffer.
	if (stringLength >= length) stringLength = length - 1;

	// Copy the string to the target buffer and write a trailing zero.
	for (i=0; i<stringLength; i++) str[i] = buf[2+i*2];
	str[stringLength] = 0;
	*/

	return 0;
}

//set configuration
/*
uint8_t usb_setConfiguration(uint8_t addr, uint8_t ep, uint8_t conf_value, unsigned int nak_limit)
{
    return(usb_controlRequest( addr, ep, bmREQ_SET, USB_REQUEST_SET_CONFIGURATION, conf_value, 0x00, 0x0000, 0x0000, NULL, USB_NAK_LIMIT));
}
*/

/**
 * Performs an in transfer from a USB device from an arbitrary endpoint.
 *
 * @param device USB bulk device.
 * @param device length number of bytes to read.
 * @param data target buffer.
 * @return number of bytes read, or error code in case of failure.
 */
int usb_read(usb_bulkDevice * device, usb_endpoint * endpoint, unsigned int length, char * data)
{
	uint8_t returnCode, bytesRead;
	uint8_t maxPacketSize = endpoint->maxPacketSize;

	unsigned int totalTransferred = 0;

	// Set device address.
	max3421e_write(MAX_REG_PERADDR, device->address);

	// Set toggle value.
	max3421e_write(MAX_REG_HCTL, endpoint->receiveToggle);

	while (1)
	{

		// Start IN transfer
		returnCode = usb_dispatchPacket2(tokIN, endpoint);
		avr_serialPrintf("dispatch %d %d %d\n", device->address, device->control.address, returnCode);
		if (returnCode) return -1;

		// Assert that the RCVDAVIRQ bit in register MAX_REG_HIRQ is set.
		// TODO: the absence of RCVDAVIRQ indicates a toggle error. Need to add handling for that.
		if ((max3421e_read(MAX_REG_HIRQ) & bmRCVDAVIRQ) == 0) return -2;

		// Obtain the number of bytes in FIFO.
		bytesRead = max3421e_read(MAX_REG_RCVBC);

		// Read the data from the FIFO.
		data = max3421e_readMultiple(MAX_REG_RCVFIFO, bytesRead, data);

		// Clear the interrupt to free the buffer.
		max3421e_write(MAX_REG_HIRQ, bmRCVDAVIRQ);

		totalTransferred += bytesRead;

		// Check if we're done reading. Either we've received a 'short' packet (<maxPacketSize), or the
		// desired number of bytes has been transferred.
		if ((bytesRead < maxPacketSize) || (totalTransferred >= length))
		{
			// Remember the toggle value for the next transfer.
			if (max3421e_read(MAX_REG_HRSL) & bmRCVTOGRD)
				endpoint->receiveToggle = bmRCVTOG1;
			else
				endpoint->receiveToggle = bmRCVTOG0;

			// Break out of the loop.
			break;
		}
	}

	// Report success.
	return totalTransferred;
}


/**
 * Performs a bulk in transfer from a USB device.
 *
 * @param device USB bulk device.
 * @param device length number of bytes to read.
 * @param data target buffer.
 * @return number of bytes read, or error code in case of failure.
 */
int usb_bulkRead(usb_bulkDevice * device, unsigned int length, char * data)
{
	return usb_read(device, &(device->in), length, data);
}


/**
 * Performs ab out transfer to a USB device on an arbitrary endpoint.
 *
 * @param device USB bulk device.
 * @param device length number of bytes to read.
 * @param data target buffer.
 * @return number of bytes read, or error code in case of failure.
 */
int usb_write(usb_bulkDevice * device, usb_endpoint * endpoint, unsigned int length, char * data)
{
	uint8_t rcode = 0, retry_count;

	// Set device address.
	max3421e_write(MAX_REG_PERADDR, device->address);

	// Local copy of the data pointer.
	char * data_p = data;

	unsigned int bytes_tosend, nak_count;
	unsigned int bytes_left = length;
	unsigned int nak_limit = USB_NAK_LIMIT;

	unsigned long timeout = avr_millis() + USB_XFER_TIMEOUT;

	uint8_t maxPacketSize = endpoint->maxPacketSize;

	if (!maxPacketSize)
	{
		//todo: move this check close to epinfo init. Make it 1< pktsize <64
		return 0xFE;
	}

	max3421e_write(MAX_REG_HCTL, endpoint->sendToggle); //set toggle value

	while (bytes_left)
	{
		retry_count = 0;
		nak_count = 0;

		bytes_tosend = (bytes_left >= maxPacketSize) ? maxPacketSize : bytes_left;

		// Filling output FIFO
		max3421e_writeMultiple(MAX_REG_SNDFIFO, bytes_tosend, data_p);

		// Set number of bytes to send.
		max3421e_write(MAX_REG_SNDBC, bytes_tosend);

		// Dispatch packet.
		max3421e_write(MAX_REG_HXFR, (tokOUT | endpoint->address));

		// Wait for completion.
		while (!(max3421e_read(MAX_REG_HIRQ) & bmHXFRDNIRQ));

		// Clear IRQ.
		max3421e_write(MAX_REG_HIRQ, bmHXFRDNIRQ);

		rcode = (max3421e_read(MAX_REG_HRSL) & 0x0f);

		while (rcode && (timeout > avr_millis()))
		{
			switch (rcode)
			{
			case hrNAK:
				nak_count++;
				if (nak_limit && (nak_count == USB_NAK_LIMIT))
				{
					return (rcode); //return NAK
				}
				break;
			case hrTIMEOUT:
				retry_count++;
				if (retry_count == USB_RETRY_LIMIT)
				{
					return (rcode); //return TIMEOUT
				}
				break;
			default:
				return (rcode);
			}//switch( rcode...
			// process NAK according to Host out NAK bug
			max3421e_write(MAX_REG_SNDBC, 0);
			max3421e_write(MAX_REG_SNDFIFO, *data_p);
			max3421e_write(MAX_REG_SNDBC, bytes_tosend);
			max3421e_write(MAX_REG_HXFR, (tokOUT | endpoint->address)); //dispatch packet
			while (!(max3421e_read(MAX_REG_HIRQ) & bmHXFRDNIRQ))
				; //wait for the completion IRQ
			max3421e_write(MAX_REG_HIRQ, bmHXFRDNIRQ); //clear IRQ
			rcode = (max3421e_read(MAX_REG_HRSL) & 0x0f);
		}//while( rcode && ....
		bytes_left -= bytes_tosend;
		data_p += bytes_tosend;
	}

	endpoint->sendToggle = (max3421e_read(MAX_REG_HRSL) & bmSNDTOGRD) ? bmSNDTOG1 : bmSNDTOG0; //update toggle

	return (rcode); //should be 0 in all cases
}

/**
 * Performs a bulk out transfer to a USB device.
 *
 * @param device USB bulk device.
 * @param device length number of bytes to read.
 * @param data target buffer.
 * @return number of bytes read, or error code in case of failure.
 */
int usb_bulkWrite(usb_bulkDevice * device, unsigned int length, char * data)
{
	return usb_write(device, &(device->out) , length, data);
}

/**
 * Read/write data to the control endpoint.
 */
uint8_t usb_ctrlData2(usb_bulkDevice * device, unsigned int length, char * data, boolean direction)
{
	if (direction)
	{ //IN transfer
		device->control.receiveToggle = bmRCVTOG1;
		return usb_read(device, &(device->control), length, data);

	} else
	{ //OUT transfer
		device->control.sendToggle = bmSNDTOG1;
		return usb_write(device, &(device->control), length, data);
	}
}

/**
 * Sends a control request to a USB device.
 *
 * @param device USB device to send the control request to.
 * @param requestType request type (in/out).
 * @param request request.
 * @param valueLow low byte of the value parameter.
 * @param valueHigh high byte of the value parameter.
 * @param index index.
 * @param length number of bytes to transfer.
 * @param data data to send in case of output transfer, or reception buffer in case of input. If no data is to be exchanged this should be set to NULL.
 */
uint8_t usb_controlRequest2(
		usb_bulkDevice * device, uint8_t requestType, uint8_t request,
		uint8_t valueLow, uint8_t valueHigh, unsigned int index, unsigned int length, char * data)
{
	boolean direction = false; //request direction, IN or OUT
	uint8_t rcode;
	usb_setupPacket setup_pkt;

	// Set device address.
	max3421e_write(MAX_REG_PERADDR, device->address);

	if (requestType & 0x80)
	{
		direction = true; //determine request direction
	}

	avr_serialPrintf("usb_getDeviceDescriptor %d %d\n", device->address, device->control.address);

	// Build setup packet.
	setup_pkt.bmRequestType = requestType;
	setup_pkt.bRequest = request;
	setup_pkt.wValue = valueLow | (valueHigh << 8);
	setup_pkt.wIndex = index;
	setup_pkt.wLength = length;

	// Write setup packet to the FIFO and dispatch
	max3421e_writeMultiple(MAX_REG_SUDFIFO, 8, (char *) &setup_pkt);
	rcode = usb_dispatchPacket2(tokSETUP, &(device->control));

	// Print error in case of failure.
	if (rcode)
	{
		avr_serialPrintf("Setup packet error: 0x%02x\n", rcode);
		return (rcode);
	}

	// Data stage, if present
	if (data != NULL)
	{
		rcode = usb_ctrlData2(device, length, data, direction);

		// If unsuccessful, return error.
		if (rcode<0)
		{
			avr_serialPrintf("Data packet error: 0x%02x\n", rcode);
			return (rcode);
		}
	}

	// Status stage.
	if (direction)
		rcode = usb_dispatchPacket2(tokOUTHS, &(device->control));
	else
		rcode = usb_dispatchPacket2(tokINHS, &(device->control));

	return (rcode);
}

uint8_t usb_getDeviceDescriptor(usb_bulkDevice * device, unsigned int nbytes, char * dataptr)
{
	avr_serialPrintf("usb_getDeviceDescriptor %d %d\n", device->address, device->control.address);
	return(usb_controlRequest2( device, bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, 0x00, USB_DESCRIPTOR_DEVICE, 0x0000, nbytes, dataptr));
}

//get configuration descriptor
uint8_t usb_getConfigurationDescriptor(usb_bulkDevice * device, unsigned int nbytes, uint8_t conf, char* dataptr, unsigned int nak_limit)
{
	return(usb_controlRequest2( device, bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, conf, USB_DESCRIPTOR_CONFIGURATION, 0x0000, nbytes, dataptr));
}

//get string descriptor
uint8_t usb_getStringDescriptor(usb_bulkDevice * device, unsigned int nbytes, uint8_t index, unsigned int langid, char * dataptr, unsigned int nak_limit)
{
    return(usb_controlRequest2( device, bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, index, USB_DESCRIPTOR_STRING, langid, nbytes, dataptr));
}

//set address
uint8_t usb_setAddress(usb_bulkDevice * device, uint8_t newaddr, unsigned int nak_limit)
{
    return(usb_controlRequest2( device, bmREQ_SET, USB_REQUEST_SET_ADDRESS, newaddr, 0x00, 0x0000, 0x0000, NULL));
}

//set configuration
uint8_t usb_setConfiguration(usb_bulkDevice * device, uint8_t conf_value, unsigned int nak_limit)
{
    return(usb_controlRequest2(device, bmREQ_SET, USB_REQUEST_SET_CONFIGURATION, conf_value, 0x00, 0x0000, 0x0000, NULL));
}
