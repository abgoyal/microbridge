/* USB functions */
#ifndef _usb_h_
#define _usb_h_

#include "max3421e.h"
#include "../ch9.h"
#include "../usb.h"

/* Common setup data constant combinations  */
#define bmREQ_GET_DESCR     USB_SETUP_DEVICE_TO_HOST|USB_SETUP_TYPE_STANDARD|USB_SETUP_RECIPIENT_DEVICE     //get descriptor request type
#define bmREQ_SET           USB_SETUP_HOST_TO_DEVICE|USB_SETUP_TYPE_STANDARD|USB_SETUP_RECIPIENT_DEVICE     //set request type for all but 'set feature' and 'set interface'
#define bmREQ_CL_GET_INTF   USB_SETUP_DEVICE_TO_HOST|USB_SETUP_TYPE_CLASS|USB_SETUP_RECIPIENT_INTERFACE     //get interface request type

/* HID requests */
/*
#define bmREQ_HIDOUT        USB_SETUP_HOST_TO_DEVICE|USB_SETUP_TYPE_CLASS|USB_SETUP_RECIPIENT_INTERFACE
#define bmREQ_HIDIN         USB_SETUP_DEVICE_TO_HOST|USB_SETUP_TYPE_CLASS|USB_SETUP_RECIPIENT_INTERFACE 
#define bmREQ_HIDREPORT     USB_SETUP_DEVICE_TO_HOST|USB_SETUP_TYPE_STANDARD|USB_SETUP_RECIPIENT_INTERFACE
*/

#define USB_XFER_TIMEOUT    5000    // USB transfer timeout in milliseconds, per section 9.2.6.1 of USB 2.0 spec
#define USB_NAK_LIMIT       32000   // NAK limit for a transfer. o meand NAKs are not counted
#define USB_RETRY_LIMIT     3       // retry limit for a transfer
#define USB_SETTLE_DELAY    200     // settle delay in milliseconds
#define USB_NAK_NOWAIT      1       // used in Richard's PS2/Wiimote code

#define USB_NUMDEVICES  2           // Number of USB devices

/* USB state machine states */

#define USB_STATE_MASK                                      0xf0

#define USB_STATE_DETACHED                                  0x10
#define USB_DETACHED_SUBSTATE_INITIALIZE                    0x11        
#define USB_DETACHED_SUBSTATE_WAIT_FOR_DEVICE               0x12
#define USB_DETACHED_SUBSTATE_ILLEGAL                       0x13
#define USB_ATTACHED_SUBSTATE_SETTLE                        0x20
#define USB_ATTACHED_SUBSTATE_RESET_DEVICE                  0x30    
#define USB_ATTACHED_SUBSTATE_WAIT_RESET_COMPLETE           0x40
#define USB_ATTACHED_SUBSTATE_WAIT_SOF                      0x50
#define USB_ATTACHED_SUBSTATE_GET_DEVICE_DESCRIPTOR_SIZE    0x60
#define USB_STATE_ADDRESSING                                0x70
#define USB_STATE_CONFIGURING                               0x80
#define USB_STATE_RUNNING                                   0x90
#define USB_STATE_ERROR                                     0xa0

// Device descriptor.
typedef struct {
	uint8_t bLength;               // Length of this descriptor.
	uint8_t bDescriptorType;       // DEVICE descriptor type (USB_DESCRIPTOR_DEVICE).
	unsigned int bcdUSB;        // USB Spec Release Number (BCD).
	uint8_t bDeviceClass;          // Class code (assigned by the USB-IF). 0xFF-Vendor specific.
	uint8_t bDeviceSubClass;       // Subclass code (assigned by the USB-IF).
	uint8_t bDeviceProtocol;       // Protocol code (assigned by the USB-IF). 0xFF-Vendor specific.
	uint8_t bMaxPacketSize0;       // Maximum packet size for endpoint 0.
	unsigned int idVendor;      // Vendor ID (assigned by the USB-IF).
	unsigned int idProduct;     // Product ID (assigned by the manufacturer).
	unsigned int bcdDevice;      // Device release number (BCD).
	uint8_t iManufacturer;         // Index of String Descriptor describing the manufacturer.
	uint8_t iProduct;              // Index of String Descriptor describing the product.
	uint8_t iSerialNumber;         // Index of String Descriptor with the device's serial number.
	uint8_t bNumConfigurations;    // Number of possible configurations.
} usb_deviceDescriptor;

// Configuration descriptor.
typedef struct
{
	uint8_t bLength;               // Length of this descriptor.
	uint8_t bDescriptorType;       // CONFIGURATION descriptor type (USB_DESCRIPTOR_CONFIGURATION).
	unsigned int wTotalLength;          // Total length of all descriptors for this configuration.
	uint8_t bNumInterfaces;        // Number of interfaces in this configuration.
	uint8_t bConfigurationValue;   // Value of this configuration (1 based).
	uint8_t iConfiguration;        // Index of String Descriptor describing the configuration.
	uint8_t bmAttributes;          // Configuration characteristics.
	uint8_t bMaxPower;             // Maximum power consumed by this configuration.
} usb_configurationDescriptor;

// Interface descriptor.
typedef struct
{
	uint8_t bLength;               // Length of this descriptor.
	uint8_t bDescriptorType;       // INTERFACE descriptor type (USB_DESCRIPTOR_INTERFACE).
	uint8_t bInterfaceNumber;      // Number of this interface (0 based).
	uint8_t bAlternateSetting;     // Value of this alternate interface setting.
	uint8_t bNumEndpoints;         // Number of endpoints in this interface.
	uint8_t bInterfaceClass;       // Class code (assigned by the USB-IF).  0xFF-Vendor specific.
	uint8_t bInterfaceSubClass;    // Subclass code (assigned by the USB-IF).
	uint8_t bInterfaceProtocol;    // Protocol code (assigned by the USB-IF).  0xFF-Vendor specific.
	uint8_t iInterface;            // Index of String Descriptor describing the interface.
} usb_interfaceDescriptor;

/* Endpoint descriptor structure */
typedef struct
{
	uint8_t bLength;               // Length of this descriptor.
	uint8_t bDescriptorType;       // ENDPOINT descriptor type (USB_DESCRIPTOR_ENDPOINT).
	uint8_t bEndpointAddress;      // Endpoint address. Bit 7 indicates direction (0=OUT, 1=IN).
	uint8_t bmAttributes;          // Endpoint transfer type.
	unsigned int wMaxPacketSize;        // Maximum packet size.
	uint8_t bInterval;             // Polling interval in frames.
} usb_endpointDescriptor;

// USB Setup Packet.
typedef struct
{
	uint8_t bmRequestType; //   0      Bit-map of request type
	uint8_t bRequest; //   1      Request
	uint16_t wValue; //   2      Depends on bRequest
	uint16_t wIndex; //   4      Depends on bRequest
	uint16_t wLength; //   6      Depends on bRequest
} usb_setupPacket;


// USB Device
typedef struct
{
    usb_endpoint* epinfo;      //device endpoint information
    uint8_t devclass;          //device class
} usb_deviceRecord;

uint8_t usb_getUsbTaskState(void);
void usb_setUsbTaskState(uint8_t state);

usb_endpoint * usb_getDevTableEntry(uint8_t addr, uint8_t ep);
void usb_setDevTableEntry(uint8_t addr, usb_endpoint * eprecord_ptr);

uint8_t usb_controlRequest(uint8_t addr, uint8_t ep, uint8_t bmReqType, uint8_t bRequest, uint8_t wValLo, uint8_t wValHi, unsigned int wInd, unsigned int nbytes, char* dataptr, unsigned int nak_limit);
uint8_t usb_ctrlStatus(uint8_t ep, boolean direction, unsigned int nak_limit);
uint8_t usb_ctrlData(uint8_t addr, uint8_t ep, unsigned int nbytes, char* dataptr, boolean direction, unsigned int nak_limit);

uint8_t usb_inTransfer(uint8_t addr, uint8_t ep, unsigned int nbytes, char* data, unsigned int nak_limit);
uint8_t usb_outTransfer(uint8_t addr, uint8_t ep, unsigned int nbytes, char* data, unsigned int nak_limit);

int usb_dispatchPacket(uint8_t token, uint8_t ep, unsigned int nak_limit);

int usb_getString(uint8_t address, uint8_t index, unsigned int length, char * str);

uint8_t usb_getDeviceDescriptor(usb_bulkDevice * device, unsigned int nbytes, char* dataptr);
uint8_t usb_getConfigurationDescriptor(usb_bulkDevice * device, unsigned int nbytes, uint8_t conf, char* dataptr, unsigned int nak_limit);
uint8_t usb_getStringDescriptor(usb_bulkDevice * device, unsigned int nbytes, uint8_t index, unsigned int langid, char * dataptr, unsigned int nak_limit);
uint8_t usb_setAddress(usb_bulkDevice * device, uint8_t newaddr, unsigned int nak_limit);
uint8_t usb_setConfiguration(usb_bulkDevice * device, uint8_t conf_value, unsigned int nak_limit);

#endif //_usb_h_
