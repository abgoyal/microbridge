#include <string.h>

#include "adb.h"

// ADB device class, subclass, and protocol
#define ADB_CLASS 0xff
#define ADB_SUBCLASS 0x42
#define ADB_PROTOCOL 0x1

#define MAX_BUF_SIZE 256

static usb_device * adbDevice;
static adb_connection connections[ADB_MAX_CONNECTIONS];
static boolean connected;

// Event handler callback function.
adb_eventHandler * eventHandler;

/**
 * Sets the ADB event handler function. This function will be called by the ADB layer
 * when interesting events occur, such as ADB connect/disconnect, connection open/close, and
 * connection writes from the ADB device.
 *
 * @param handler event handler function.
 */
void adb_setEventHandler(adb_eventHandler * handler)
{
	eventHandler = handler;
}

/**
 * Fires an ADB event.
 * @param connection ADB connection. May be NULL in case of global connect/disconnect events.
 * @param type event type.
 * @param length payload length or zero if no payload.
 * @param data payload data if relevant or NULL otherwise.
 */
static void adb_fireEvent(adb_connection * connection, adb_eventType type, uint16_t length, char * data)
{
	eventHandler(connection, type, length, data);
}

/**
 * Adds a new ADB connection. The connection string is per ADB specs, for example "tcp:1234" opens a
 * connection to tcp port 1234, and "shell:ls" outputs a listing of the phone root filesystem. Connections
 * can be made persistent by setting reconnect to true. Persistent connections will be automatically
 * reconnected when the USB cable is re-plugged in. Non-persistent connections will connect only once,
 * and should never be used after they are closed.
 *
 * @param connectionString ADB connectionstring. I.e. "tcp:1234" or "shell:ls".
 * @param reconnect true for automatic reconnect (persistent connections).
 * @return an ADB connection record or NULL on failure (not enough slots).
 */
adb_connection * adb_addConnection(char * connectionString, boolean reconnect)
{
	int index;

	// Find an empty spot in the connection list.
	for (index=0; index<ADB_MAX_CONNECTIONS; index++)
		if (connections[index].status==ADB_UNUSED)
		{
			snprintf(connections[index].connectionString, ADB_CONNECTSTRING_LENGTH, "%s", connectionString);
			connections[index].localID = index + 1; // local id may not be zero
			connections[index].status = ADB_CLOSED;
			connections[index].lastConnectionAttempt = 0;
			connections[index].reconnect = reconnect;

			return &connections[index];
		}

	// Unable to find an empty spot, all connection slots in use.
	return NULL;
}

/**
 * Writes an empty message (without payload) to the ADB device.
 *
 * @param device USB device handle.
 * @param command ADB command.
 * @param arg0 first ADB argument (command dependent).
 * @param arg0 second ADB argument (command dependent).
 * @return error code or 0 for success.
 */
static int adb_writeEmptyMessage(usb_device * device, uint32_t command, uint32_t arg0, uint32_t arg1)
{
	adb_message message;

	message.command = command;
	message.arg0 = arg0;
	message.arg1 = arg1;
	message.data_length = 0;
	message.data_check = 0;
	message.magic = command ^ 0xffffffff;

	return usb_bulkWrite(device, sizeof(adb_message), (uint8_t*)&message);
}

/**
 * Writes an ADB message with payload to the ADB device.
 *
 * @param device USB device handle.
 * @param command ADB command.
 * @param arg0 first ADB argument (command dependent).
 * @param arg0 second ADB argument (command dependent).
 * @param length payload length.
 * @param data command payload.
 * @return error code or 0 for success.
 */
int adb_writeMessage(usb_device * device, uint32_t command, uint32_t arg0, uint32_t arg1, uint32_t length, uint8_t * data)
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

	rcode = usb_bulkWrite(device, sizeof(adb_message), (uint8_t*)&message);
	if (rcode) return rcode;

	rcode = usb_bulkWrite(device, length, data);

	return rcode;

}

/**
 * Writes an ADB command with a string as payload.
 *
 * @param device USB device handle.
 * @param command ADB command.
 * @param arg0 first ADB argument (command dependent).
 * @param arg0 second ADB argument (command dependent).
 * @param str payload string.
 * @return error code or 0 for success.
 */
int adb_writeStringMessage(usb_device * device, uint32_t command, uint32_t arg0, uint32_t arg1, char * str)
{
	return adb_writeMessage(device, command, arg0, arg1, strlen(str) + 1, (uint8_t*)str);
}

/**
 * Sends an ADB OPEN message for any connections that are currently in the CLOSED state.
 */
static void adb_openClosedConnections()
{
	uint32_t timeSinceLastConnect;
	int i;

	// Iterate over the connection list and send "OPEN" for the ones that are currently closed.
	for (i=0; i<ADB_MAX_CONNECTIONS; i++)
	{
		timeSinceLastConnect = avr_millis() - connections[i].lastConnectionAttempt;
		if (connections[i].status==ADB_CLOSED && timeSinceLastConnect>ADB_CONNECTION_RETRY_TIME)
		{
			// Issue open command.
			adb_writeStringMessage(adbDevice, A_OPEN, connections[i].localID, 0, connections[i].connectionString);

			// Record the last attempt time
			connections[i].lastConnectionAttempt = avr_millis();
			connections[i].status = ADB_OPENING;
		}
	}

}

/**
 * Handles and ADB OKAY message.
 *
 * @param connection ADB connection
 * @param message ADB message struct.
 */
static void adb_handleOkay(adb_connection * connection, adb_message * message)
{
	// Check if the OKAY message was a response to a CONNECT message.
	if (connection->status==ADB_OPENING)
	{
		connection->status = ADB_OPEN;
		connection->remoteID = message->arg0;

		adb_fireEvent(connection, ADB_CONNECTION_OPEN, 0, NULL);
	}

	// Check if the OKAY message was a response to a WRITE message.
	if (connection->status==ADB_WRITING)
		connection->status = ADB_OPEN;

}

/**
 * Handles an ADB CLOSE message, and fires an ADB event accordingly.
 *
 * @param connection ADB connection
 */
static void adb_handleClose(adb_connection * connection)
{
	// Check if the CLOSE message was a response to a CONNECT message.
	if (connection->status==ADB_OPENING)
		adb_fireEvent(connection, ADB_CONNECTION_FAILED, 0, NULL);
	else
		adb_fireEvent(connection, ADB_CONNECTION_CLOSE, 0, NULL);

	// Connection failed
	if (connection->reconnect)
		connection->status = ADB_CLOSED;
	else
		connection->status = ADB_UNUSED;

}

/**
 * Handles an ADB WRITE message.
 *
 * @param connection ADB connection
 * @param message ADB message struct.
 */
static void adb_handleWrite(adb_connection * connection, adb_message * message)
{
	uint32_t bytesLeft = message->data_length;
	uint8_t buf[ADB_USB_PACKETSIZE];
	int bytesRead;

	connection->status = ADB_RECEIVING;
	connection->dataRead = 0;
	connection->dataSize = message->data_length;

	while (bytesLeft>0)
	{
		int len = bytesLeft < ADB_USB_PACKETSIZE ? bytesLeft : ADB_USB_PACKETSIZE;

		// Read payload
		bytesRead = usb_bulkRead(adbDevice, len, buf, false);
		// TODO	error if (bytesRead < 0)

		connection->dataRead += len;
		adb_fireEvent(connection, ADB_CONNECTION_RECEIVE, len, (char*)buf);

		bytesLeft -= bytesRead;
	}

	// Send OKAY message in reply.
	bytesRead = adb_writeEmptyMessage(adbDevice, A_OKAY, message->arg1, message->arg0);
	connection->status = ADB_OPEN;
}

/**
 * Close all ADB connections.
 *
 * @param connection ADB connection
 * @param message ADB message struct.
 */
static void adb_closeAll()
{
	int i;

	// Iterate over all connections and close the ones that are currently open.
	for (i=0; i<ADB_MAX_CONNECTIONS; i++)
		if (!(connections[i].status==ADB_UNUSED || connections[i].status==ADB_CLOSED))
			adb_handleClose(&connections[i]);

}

/**
 * Poll an ADB message.
 */
static boolean adb_pollMessage(adb_message * message)
{
	int bytesRead;

	// Poll a packet from the USB
	bytesRead = usb_bulkRead(adbDevice, ADB_USB_PACKETSIZE, (uint8_t*)message, true);

	// Check if the USB in transfer was successful.
	if (bytesRead<0) return false;

	// Check if the received number of bytes matches our expected 24 bytes of ADB message header.
	if (bytesRead != sizeof(adb_message)) return false;

	return true;
}

static void adb_handleConnect(adb_message * message)
{
	unsigned int bytesRead;
	char buf[MAX_BUF_SIZE];
	uint16_t len;

	// Read payload (remote ADB device ID)
	len = message->data_length < MAX_BUF_SIZE ? message->data_length : MAX_BUF_SIZE;
	bytesRead = usb_bulkRead(adbDevice, len, buf, false);

	connected = true;

	adb_fireEvent(NULL, ADB_CONNECT, len, buf);

}

void adb_poll()
{
	int i;
	adb_message message;

	// Poll the USB layer.
	usb_poll();

	// If no USB device, there's no work for us to be done, so just return.
	if (adbDevice==NULL) return;

	// If not connected, send a connection string to the device.
	if (!connected)
	{
		adb_writeStringMessage(adbDevice, A_CNXN, 0x01000000, 4096, "host::microbridge");
		avr_delay(500);
	}

	// If we are connected, check if there are connections that need to be opened
	if (connected)
		adb_openClosedConnections();

	// Check for an incoming ADB message.
	if (!adb_pollMessage(&message))
		return;

//	switch (message.command)
//	{
//	case A_CNXN:
//			avr_serialPrintf("ADB CONNECTION command\n");
//			break;
//	case A_OKAY:
//			avr_serialPrintf("ADB OKAY command, local-id: %ld, remote-id:%ld\n", message.arg0, message.arg1);
//			break;
//	case A_OPEN:
//			avr_serialPrintf("ADB OPEN command\n");
//			break;
//	case A_CLSE:
//			avr_serialPrintf("ADB CLOSE command\n");
//			break;
//	case A_SYNC:
//			avr_serialPrintf("ADB SYNC command\n");
//			break;
//	case A_WRTE:
//			avr_serialPrintf("ADB WRITE command, local-id: %ld, remote-id:%ld\n", message.arg0, message.arg1);
//			break;
//	default:
//			avr_serialPrintf("Unknown ADB command: %lx\n", message.command);
//			break;
//	}

	// Handle a response from the ADB device to our CONNECT message.
	if (message.command==A_CNXN)
		adb_handleConnect(&message);

	// Handle messages for specific connections
	for (i=0; i<ADB_MAX_CONNECTIONS; i++)
	{
		if (connections[i].status!=ADB_UNUSED && connections[i].localID==message.arg1)
		{
			switch(message.command)
			{
			case A_OKAY:
				adb_handleOkay(&(connections[i]), &message);
				break;
			case A_CLSE:
				adb_handleClose(&(connections[i]));
				break;
			case A_WRTE:
				adb_handleWrite(&(connections[i]), &message);
				break;
			default:
				break;
			}
		}
	}

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
 * Checks whether the a connected USB device is an ADB device and populates a configuration record if it is.
 *
 * @param device USB device.
 * @param handle pointer to a configuration record. The endpoint device address, configuration, and endpoint information will be stored here.
 * @return true iff the device is an ADB device.
 */
static boolean adb_isAdbDevice(usb_device * device, int configuration, adb_usbConfiguration * handle)
{
	boolean ret = false;
	uint8_t buf[MAX_BUF_SIZE];
	int bytesRead;

	// Read the length of the configuration descriptor.
	bytesRead = usb_getConfigurationDescriptor(device, configuration, MAX_BUF_SIZE, buf);
	if (bytesRead<0) return false;

	uint16_t pos = 0;
	uint8_t descriptorLength;
	uint8_t descriptorType;

	usb_configurationDescriptor * config = NULL;
	usb_interfaceDescriptor * interface = NULL;
	usb_endpointDescriptor * endpoint = NULL;

	while (pos < bytesRead)
	{
		descriptorLength = buf[pos];
		descriptorType = buf[pos + 1];

		switch (descriptorType)
		{
		case (USB_DESCRIPTOR_CONFIGURATION):
			config = (usb_configurationDescriptor *)(buf + pos);
			break;
		case (USB_DESCRIPTOR_INTERFACE):
			interface = (usb_interfaceDescriptor *)(buf + pos);

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

			// If this endpoint descriptor is found right after the ADB interface descriptor, it belong to that interface.
			if (interface->bInterfaceNumber == handle->interface)
			{
				if (endpoint->bEndpointAddress & 0x80)
					handle->inputEndPointAddress = endpoint->bEndpointAddress & ~0x80;
				else
					handle->outputEndPointAddress = endpoint->bEndpointAddress;
			}

			break;
		default:
			break;
		}

		pos += descriptorLength;
	}

	return ret;

}

/**
 * Initialises an ADB device.
 *
 * @param device the USB device.
 * @param configuration configuration information.
 */
static void adb_initUsb(usb_device * device, adb_usbConfiguration * handle)
{
	// Initialise/configure the USB device.
	// TODO write a usb_initBulkDevice function?
	usb_initDevice(device, handle->configuration);

	// Initialise bulk input endpoint.
	usb_initEndPoint(&(device->bulk_in), handle->inputEndPointAddress);
	device->bulk_in.attributes = USB_TRANSFER_TYPE_BULK;
	device->bulk_in.maxPacketSize = ADB_USB_PACKETSIZE;

	// Initialise bulk output endpoint.
	usb_initEndPoint(&(device->bulk_out), handle->outputEndPointAddress);
	device->bulk_out.attributes = USB_TRANSFER_TYPE_BULK;
	device->bulk_out.maxPacketSize = ADB_USB_PACKETSIZE;

	// Success, signal that we are now connected.
	adbDevice = device;
}

// USB event handler. Detects device connect/disconnect and configures the device
// if it's an ADB device.
static void adb_usbEventHandler(usb_device * device, usb_eventType event)
{
	adb_usbConfiguration handle;

	switch (event)
	{
	case USB_CONNECT:

		// Check if the newly connected device is an ADB device, and initialise it if so.
		if (adb_isAdbDevice(device, 0, &handle))
			adb_initUsb(device, &handle);

		break;

	case USB_DISCONNECT:

		// Check if the device that was disconnected is the ADB device we've been using.
		if (device == adbDevice)
		{
			// Close all open ADB connections.
			adb_closeAll();

			// Signal that we're no longer connected by setting the global device handler to NULL;
			adbDevice = NULL;
			connected = false;
		}

		break;

	default:
		// ignore
		break;
	}
}

int adb_write(adb_connection * connection, uint16_t length, uint8_t * data)
{
	int ret;

	// First check if we have a working ADB connection
	if (adbDevice==NULL || !connected) return -1;

	// Check if the connection is open for writing.
	if (connection->status != ADB_OPEN) return -2;

	// Write payload
	connection->status = ADB_WRITING;
	ret = adb_writeMessage(adbDevice, A_WRTE, connection->localID, connection->remoteID, length, data);
	// avr_serialPrintf("err %d\n", ret);

	return ret;
}

int adb_writeString(adb_connection * connection, char * str)
{
	int ret;

	// First check if we have a working ADB connection
	if (adbDevice==NULL || !connected) return -1;

	// Check if the connection is open for writing.
	if (connection->status != ADB_OPEN) return -2;

	// Write payload
	connection->status = ADB_WRITING;
	ret = adb_writeStringMessage(adbDevice, A_WRTE, connection->localID, connection->remoteID, str);
	// avr_serialPrintf("err %d\n", ret);

	return ret;
}

/**
 * Initialises the ADB protocol. This function initialises the USB layer itsel,
 * so no further setup is required.
 */
void adb_init()
{
	// Signal that we are not connected.
	adbDevice = NULL;
	connected = false;

	// Initialise the USB layer and attach an event handler.
	usb_setEventHandler(adb_usbEventHandler);
	usb_init();
}
