/* USB functions */
#include "../avr.h"
#include "max3421e_usb.h"
#include "../ch9.h"

static uint8_t usb_error = 0;
static uint8_t usb_task_state = USB_DETACHED_SUBSTATE_INITIALIZE;

usb_deviceRecord devtable[USB_NUMDEVICES + 1];

usb_endpoint dev0ep; //Endpoint data structure used during enumeration for uninitialized device

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

	// Set single ep for uninitialized device
	devtable[0].epinfo = &dev0ep;

	// not necessary dev0ep.MaxPktSize = 8;          //minimum possible
	dev0ep.sndToggle = bmSNDTOG0; //set DATA0/1 toggles to 0
	dev0ep.rcvToggle = bmRCVTOG0;
}

uint8_t usb_getUsbTaskState(void)
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

/* Control transfer. Sets address, endpoint, fills control packet with necessary data, dispatches control packet, and initiates bulk IN transfer,   */
/* depending on request. Actual requests are defined as inlines                                                                                      */
/* return codes:                */
/* 00       =   success         */
/* 01-0f    =   non-zero HRSLT  */
uint8_t usb_controlRequest(
		uint8_t addr,
		uint8_t ep,
		uint8_t bmReqType,
		uint8_t bRequest,
		uint8_t wValLo,
		uint8_t wValHi,
		unsigned int wInd,
		unsigned int nbytes,
		char* dataptr,
		unsigned int nak_limit
		)
{
	boolean direction = false; //request direction, IN or OUT
	uint8_t rcode;
	usb_SetupPacket setup_pkt;

	max3421e_write(PERADDR, addr); //set peripheral address
	if (bmReqType & 0x80)
	{
		direction = true; //determine request direction
	}

	/* fill in setup packet */
	setup_pkt.bmRequestType = bmReqType;
	setup_pkt.bRequest = bRequest;
	setup_pkt.wValue = wValLo | (wValHi << 8);

	setup_pkt.wIndex = wInd;
	setup_pkt.wLength = nbytes;
	max3421e_writeMultiple(SUDFIFO, 8, (char *) &setup_pkt); //transfer to setup packet FIFO
	rcode = usb_dispatchPacket(tokSETUP, ep, nak_limit); //dispatch packet

	//Serial.println("Setup packet");   //DEBUG
	if (rcode)
	{ //return HRSLT if not zero
		avr_serialPrintf("Setup packet error: 0x%02x\n", rcode);
		return (rcode);
	}

	if (dataptr != NULL)
	{ //data stage, if present
		rcode = usb_ctrlData(addr, ep, nbytes, dataptr, direction, USB_NAK_LIMIT);
	}

	if (rcode)
	{ //return error
		avr_serialPrintf("Data packet error: 0x%02x\n", rcode);
		return (rcode);
	}
	rcode = usb_ctrlStatus(ep, direction, USB_NAK_LIMIT); //status stage
	return (rcode);
}

/* Control transfer with status stage and no data stage */
/* Assumed peripheral address is already set */
uint8_t usb_ctrlStatus(uint8_t ep, boolean direction, unsigned int nak_limit)
{
	uint8_t rcode;
	if (direction)
	{ //GET
		rcode = usb_dispatchPacket(tokOUTHS, ep, nak_limit);
	} else
	{
		rcode = usb_dispatchPacket(tokINHS, ep, nak_limit);
	}
	return (rcode);
}

/* Control transfer with data stage. Stages 2 and 3 of control transfer. Assumes preipheral address is set and setup packet has been sent */
uint8_t usb_ctrlData(uint8_t addr, uint8_t ep, unsigned int nbytes, char* dataptr, boolean direction, unsigned int nak_limit)
{
	uint8_t rcode;
	if (direction)
	{ //IN transfer
		devtable[addr].epinfo[ep].rcvToggle = bmRCVTOG1;
		rcode = usb_inTransfer(addr, ep, nbytes, dataptr, nak_limit);
		return (rcode);
	} else
	{ //OUT transfer
		avr_serialPrintf("Sending OUT message\n");
		devtable[addr].epinfo[ep].sndToggle = bmSNDTOG1;
		rcode = usb_outTransfer(addr, ep, nbytes, dataptr, nak_limit);
		return (rcode);
	}
}

/* IN transfer to arbitrary endpoint. Assumes PERADDR is set. Handles multiple packets if necessary. Transfers 'nbytes' uint8_ts. */
/* Keep sending INs and writes data to memory area pointed by 'data'                                                           */
/* rcode 0 if no errors. rcode 01-0f is relayed from usb_dispatchPkt(). Rcode f0 means RCVDAVIRQ error,
 fe USB xfer timeout */

/**
 * Performs an IN transfer. Assumes PERADDR (device address) is set.
 *
 */
uint8_t usb_inTransfer(uint8_t address, uint8_t endpoint, unsigned int nbytes, char * data, unsigned int nak_limit)
{
	uint8_t rcode;
	uint8_t pktsize;
	uint8_t maxpktsize = devtable[address].epinfo[endpoint].MaxPktSize;
	uint8_t endpointAddress = devtable[address].epinfo[endpoint].address;

	unsigned int xfrlen = 0;
	max3421e_write(HCTL, devtable[address].epinfo[endpoint].rcvToggle); //set toggle value
	while (1)
	{

		// Start IN transfer
		rcode = usb_dispatchPacket(tokIN, endpointAddress, nak_limit); //IN packet to EP-'endpoint'. Function takes care of NAKS.
		if (rcode) return (rcode);

		/* check for RCVDAVIRQ and generate error if not present */
		/* the only case when absense of RCVDAVIRQ makes sense is when toggle error occured. Need to add handling for that */
		if ((max3421e_read(HIRQ) & bmRCVDAVIRQ) == 0)
		{
			return (0xf0); //receive error
		}

		pktsize = max3421e_read(RCVBC); //number of received uint8_ts

		data = max3421e_readMultiple(RCVFIFO, pktsize, data);
		max3421e_write(HIRQ, bmRCVDAVIRQ); // Clear the IRQ & free the buffer
		xfrlen += pktsize; // add this packet's uint8_t count to total transfer length
		/* The transfer is complete under two conditions:           */
		/* 1. The device sent a short packet (L.T. maxPacketSize)   */
		/* 2. 'nbytes' have been transferred.                       */
		if ((pktsize < maxpktsize) || (xfrlen >= nbytes))
		{ // have we transferred 'nbytes' uint8_ts?
			if (max3421e_read(HRSL) & bmRCVTOGRD)
			{ //save toggle value
				devtable[address].epinfo[endpoint].rcvToggle = bmRCVTOG1;
			} else
			{
				devtable[address].epinfo[endpoint].rcvToggle = bmRCVTOG0;
			}
			return (0);
		}
	}//while( 1 )
}

/* OUT transfer to arbitrary endpoint. Assumes PERADDR is set. Handles multiple packets if necessary. Transfers 'nbytes' uint8_ts. */
/* Handles NAK bug per Maxim Application Note 4000 for single buffer transfer   */
/* rcode 0 if no errors. rcode 01-0f is relayed from HRSL                       */
/* major part of this function borrowed from code shared by Richard Ibbotson    */
uint8_t usb_outTransfer(uint8_t addr, uint8_t ep, unsigned int nbytes, char* data, unsigned int nak_limit)
{
	uint8_t rcode = 0, retry_count;
	char* data_p = data; //local copy of the data pointer
	unsigned int bytes_tosend, nak_count;
	unsigned int bytes_left = nbytes;
	uint8_t maxpktsize = devtable[addr].epinfo[ep].MaxPktSize;
	unsigned long timeout = avr_millis() + USB_XFER_TIMEOUT;

	if (!maxpktsize)
	{
		//todo: move this check close to epinfo init. Make it 1< pktsize <64
		return 0xFE;
	}

	max3421e_write(HCTL, devtable[addr].epinfo[ep].sndToggle); //set toggle value

	while (bytes_left)
	{
		retry_count = 0;
		nak_count = 0;

		bytes_tosend = (bytes_left >= maxpktsize) ? maxpktsize : bytes_left;

		// Filling output FIFO
		max3421e_writeMultiple(SNDFIFO, bytes_tosend, data_p);

		// Set number of bytes to send.
		max3421e_write(SNDBC, bytes_tosend);

		// Dispatch packet.
		max3421e_write(HXFR, (tokOUT | devtable[addr].epinfo[ep].address));

		// Wait for completion.
		while (!(max3421e_read(HIRQ) & bmHXFRDNIRQ));

		// Clear IRQ.
		max3421e_write(HIRQ, bmHXFRDNIRQ);

		rcode = (max3421e_read(HRSL) & 0x0f);

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
			/* process NAK according to Host out NAK bug */
			max3421e_write(SNDBC, 0);
			max3421e_write(SNDFIFO, *data_p);
			max3421e_write(SNDBC, bytes_tosend);
			max3421e_write(HXFR, (tokOUT | devtable[addr].epinfo[ep].address)); //dispatch packet
			while (!(max3421e_read(HIRQ) & bmHXFRDNIRQ))
				; //wait for the completion IRQ
			max3421e_write(HIRQ, bmHXFRDNIRQ); //clear IRQ
			rcode = (max3421e_read(HRSL) & 0x0f);
		}//while( rcode && ....
		bytes_left -= bytes_tosend;
		data_p += bytes_tosend;
	}

	devtable[addr].epinfo[ep].sndToggle = (max3421e_read(HRSL) & bmSNDTOGRD) ? bmSNDTOG1 : bmSNDTOG0; //update toggle

	return (rcode); //should be 0 in all cases
}

/**
 * Dispatches a USB packet. Assumes that the peripheral address is set and the relevant buffer is loaded/empty.
 *
 * If a NAK is received, the packet is resent up to [nak_limit] times. A nak_limit of zero indicates that
 * the method should return after timeout rather than counting NAKs.
 *
 * In case of a bus timeout, re-sends up to USB_RETRY_LIMIT times.
 *
 * @param endpointAddress endpoint address.
 * @return 0x00-0x0f as HRSLT, 0x00 is success, 0xff is timeout.
 */
int usb_dispatchPacket(uint8_t token, uint8_t endpointAddress, unsigned int nak_limit)
{
	unsigned long timeout = avr_millis() + USB_XFER_TIMEOUT;
	uint8_t tmpdata;
	uint8_t rcode = 0;
	unsigned int nak_count = 0;
	char retry_count = 0;

	while (timeout > avr_millis())
	{
		max3421e_write(HXFR, (token | endpointAddress)); //launch the transfer
		rcode = 0xff;
		while (avr_millis() < timeout)
		{ //wait for transfer completion
			tmpdata = max3421e_read(HIRQ);
			if (tmpdata & bmHXFRDNIRQ)
			{
				max3421e_write(HIRQ, bmHXFRDNIRQ); //clear the interrupt
				rcode = 0x00;
				break;
			}//if( tmpdata & bmHXFRDNIRQ
		}//while ( avr_millis() < timeout
		if (rcode != 0x00)
		{ //exit if timeout
			return (rcode);
		}
		rcode = (max3421e_read(HRSL) & 0x0f); //analyze transfer result
		switch (rcode)
		{
		case hrNAK:
			nak_count++;
			if (nak_limit && (nak_count == nak_limit))
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
	usb_DeviceDescriptor buf;
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
		max3421e_write(HCTL, bmBUSRST); //issue bus reset
		usb_task_state = USB_ATTACHED_SUBSTATE_WAIT_RESET_COMPLETE;
		break;
	case USB_ATTACHED_SUBSTATE_WAIT_RESET_COMPLETE:
		if ((max3421e_read(HCTL) & bmBUSRST) == 0)
		{
			tmpdata = max3421e_read(MODE) | bmSOFKAENAB; //start SOF generation
			max3421e_write(MODE, tmpdata);
			//                  max3421e_regWr( rMODE, bmSOFKAENAB );
			usb_task_state = USB_ATTACHED_SUBSTATE_WAIT_SOF;
			delay = avr_millis() + 20; //20ms wait after reset per USB spec
		}
		break;
	case USB_ATTACHED_SUBSTATE_WAIT_SOF: //todo: change check order
		if (max3421e_read(HIRQ) & bmFRAMEIRQ)
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
		devtable[0].epinfo->MaxPktSize = 8; //set max.packet size to min.allowed
		rcode = usb_getDeviceDescriptor(0, 0, 8, (char*) &buf, USB_NAK_LIMIT);
		if (rcode == 0)
		{
			devtable[0].epinfo->MaxPktSize = buf.bMaxPacketSize0;
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
				rcode = usb_setAddress(0, 0, i, USB_NAK_LIMIT);
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
	// Buffer size hardcoded to 128
	char buf[128];
	uint8_t stringLength = 0;
	int i, ret;

	// avr_serialPrintf("getString %d %d %d\n", address, index, length);

	// Get the first byte of the string descriptor, which contains the length.
	ret = usb_getStringDescriptor(address, 0x0 /* control pipe, always 0 */, sizeof(uint8_t), index, 0x0 /* language */, (char*)&stringLength, USB_NAK_LIMIT);
	if (ret) return ret;
	if (stringLength>128) stringLength = 128;

	// avr_serialPrintf("getDescriptor\n");

	// Get the whole thing.
	ret = usb_getStringDescriptor(address, 0x0 /* control pipe, always 0 */, stringLength, index, 0x0 /* language */, buf, USB_NAK_LIMIT);
	if (ret) return ret;
	stringLength = (stringLength - 2) / 2;

	// Don't write outside the bounds of the target buffer.
	if (stringLength >= length) stringLength = length - 1;

	// Copy the string to the target buffer and write a trailing zero.
	for (i=0; i<stringLength; i++) str[i] = buf[2+i*2];
	str[stringLength] = 0;

	return 0;
}

/**
 * USB bulk read.
 */
int usb_bulkRead(uint8_t address, usb_endpoint * endpoint, unsigned int length, char * data, unsigned int * bytesRead)
{
	uint8_t rcode;
	uint8_t pktsize;
	uint8_t maxpktsize = endpoint->MaxPktSize;

	unsigned int xfrlen = 0;
	max3421e_write(HCTL, endpoint->rcvToggle); //set toggle value
	while (1)
	{

		// Start IN transfer
		rcode = usb_dispatchPacket(tokIN, endpoint->address, USB_NAK_LIMIT); //IN packet to EP-'endpoint'. Function takes care of NAKS.
		if (rcode) return (rcode);

		/* check for RCVDAVIRQ and generate error if not present */
		/* the only case when absense of RCVDAVIRQ makes sense is when toggle error occured. Need to add handling for that */
		if ((max3421e_read(HIRQ) & bmRCVDAVIRQ) == 0)
		{
			return (0xf0); //receive error
		}

		pktsize = max3421e_read(RCVBC); //number of received uint8_ts

		data = max3421e_readMultiple(RCVFIFO, pktsize, data);
		max3421e_write(HIRQ, bmRCVDAVIRQ); // Clear the IRQ & free the buffer
		xfrlen += pktsize; // add this packet's uint8_t count to total transfer length

		/* The transfer is complete under two conditions:           */
		/* 1. The device sent a short packet (L.T. maxPacketSize)   */
		/* 2. 'length' have been transferred.                       */
		if ((pktsize < maxpktsize) || (xfrlen >= length))
		{ // have we transferred 'length' uint8_ts?
			if (max3421e_read(HRSL) & bmRCVTOGRD)
			{ //save toggle value
				endpoint->rcvToggle = bmRCVTOG1;
			} else
			{
				endpoint->rcvToggle = bmRCVTOG0;
			}
			break;
		}
	}

	*bytesRead = pktsize;
	return 0;
}
