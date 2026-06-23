#ifndef USB_H
#define USB_H

#include "harlin_API.h"

#define USB_CLASS_UHCI    0x0C
#define USB_SUBCLASS_UHCI 0x03
#define USB_PROGIF_UHCI   0x00

#define USB_DIR_OUT       0
#define USB_DIR_IN        1

#define USB_REQ_GET_DESC          0x06
#define USB_REQ_SET_ADDR          0x05
#define USB_REQ_SET_CONFIG        0x09

#define USB_DESC_DEVICE   1
#define USB_DESC_CONFIG   2

#define USB_PID_SETUP     0x2D
#define USB_PID_IN        0x69
#define USB_PID_OUT       0xE1

#define UHCI_CMD          0x00
#define UHCI_STS          0x02
#define UHCI_INTR         0x04
#define UHCI_FRNUM        0x06
#define UHCI_FLBASEADD    0x08
#define UHCI_SOFMOD       0x0C
#define UHCI_PORTSC1      0x10
#define UHCI_PORTSC2      0x12

#define UHCI_CMD_RS       (1 << 0)
#define UHCI_CMD_HCRESET  (1 << 1)
#define UHCI_CMD_GRESET   (1 << 2)
#define UHCI_CMD_CF       (1 << 6)
#define UHCI_CMD_MAXP     (1 << 7)

#define UHCI_STS_HCH      (1 << 5)

#define UHCI_PORT_CCS     (1 << 0)
#define UHCI_PORT_CSC     (1 << 1)
#define UHCI_PORT_PED     (1 << 2)
#define UHCI_PORT_PEDC    (1 << 3)
#define UHCI_PORT_PR      (1 << 6)
#define UHCI_PORT_LSDA    (1 << 7)

#define UHCI_FRAME_LIST_SIZE 1024

#define UHCI_TD_ACTIVE       (1 << 31)
#define UHCI_TD_ERROR_MASK   0x7E000000
#define UHCI_TD_STALLED      (1 << 26)
#define UHCI_TD_NAK          (1 << 27)
#define UHCI_TD_CRC          (1 << 28)
#define UHCI_TD_IOC          (1 << 23)
#define UHCI_TD_LS           (1 << 25)

struct uhci_td {
    u32 link;
    u32 status;
    u32 token;
    u32 buffer;
} __attribute__((packed));

struct uhci_qh {
    u32 link;
    u32 element;
} __attribute__((packed));

struct uhci_frentry {
    u32 ptr;
} __attribute__((packed));

struct uhci_controller {
    int present;
    u16 io_base;
    u8 irq;
    u8 bus, dev, func;
    struct uhci_frentry* frame_list;
    u64 frame_list_phys;
};

struct usb_device {
    int present;
    u8 address;
    u8 port;
    u8 speed;
    u16 vendor_id;
    u16 product_id;
    u8 max_packet_size;
    u8 class_code;
    u8 subclass;
    u8 protocol;
    int controller_index;
};

struct usb_setup_pkt {
    u8 bmRequestType;
    u8 bRequest;
    u16 wValue;
    u16 wIndex;
    u16 wLength;
} __attribute__((packed));

struct usb_device_desc {
    u8  bLength;
    u8  bDescriptorType;
    u16 bcdUSB;
    u8  bDeviceClass;
    u8  bDeviceSubClass;
    u8  bDeviceProtocol;
    u8  bMaxPacketSize0;
    u16 idVendor;
    u16 idProduct;
    u16 bcdDevice;
    u8  iManufacturer;
    u8  iProduct;
    u8  iSerialNumber;
    u8  bNumConfigurations;
} __attribute__((packed));

#define USB_MAX_CONTROLLERS  4
#define USB_MAX_DEVICES      16

int  usb_init(void);
int  usb_controller_count(void);
int  usb_device_count(void);
int  usb_get_device(int index, struct usb_device* out);

#define Harlin_UsbInit                usb_init
#define Harlin_UsbControllerCount     usb_controller_count
#define Harlin_UsbDeviceCount         usb_device_count
#define Harlin_UsbGetDevice           usb_get_device

#endif
