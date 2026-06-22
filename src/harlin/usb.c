#include "usb.h"
#include "pci.h"
#include "io.h"
#include "pmm.h"
#include "screen.h"

#define UHCI_TOKEN(addr, endpt, pid, toggle, maxlen) \
    (((u32)(addr) & 0x7f) | (((u32)(endpt) & 0x0f) << 7) | \
     (((u32)(pid) & 0xff) << 8) | (((u32)(toggle) & 1) << 11) | \
     (((u32)(maxlen) - 1) << 21))

#define UHCI_TD_LINK(td_phys, depth, terminate) \
    (((u32)(td_phys) & 0xFFFFFFF0) | \
     ((depth) ? (1 << 2) : 0) | \
     ((terminate) ? 1 : 0))

#define UHCI_QH_LINK(qh_phys, terminate) \
    (((u32)(qh_phys) & 0xFFFFFFF0) | (1 << 1) | ((terminate) ? 1 : 0))

#define UHCI_FRENTRY(frame_phys, terminate) \
    (((u32)(frame_phys) & 0xFFFFFFF0) | ((terminate) ? 1 : 0))

static struct uhci_controller controllers[USB_MAX_CONTROLLERS];
static int controller_count;
static struct usb_device devices[USB_MAX_DEVICES];
static int device_count;

static u16 uhci_readw(struct uhci_controller* ctrl, u16 reg)
{
    return inw(ctrl->io_base + reg);
}

static void uhci_writew(struct uhci_controller* ctrl, u16 reg, u16 val)
{
    outw(ctrl->io_base + reg, val);
}

static void uhci_writel(struct uhci_controller* ctrl, u16 reg, u32 val)
{
    outl(ctrl->io_base + reg, val);
}

static int uhci_reset(struct uhci_controller* ctrl)
{
    int timeout;

    uhci_writew(ctrl, UHCI_CMD, UHCI_CMD_HCRESET);
    for (timeout = 0; timeout < 1000; timeout++) {
        if (!(uhci_readw(ctrl, UHCI_CMD) & UHCI_CMD_HCRESET))
            break;
        inb(0x80);
        inb(0x80);
    }
    if (timeout >= 1000)
        return -1;

    for (timeout = 0; timeout < 1000; timeout++) {
        if (uhci_readw(ctrl, UHCI_STS) & UHCI_STS_HCH)
            break;
        inb(0x80);
    }
    if (timeout >= 1000)
        return -1;

    return 0;
}

static int uhci_start(struct uhci_controller* ctrl)
{
    uhci_writew(ctrl, UHCI_CMD, UHCI_CMD_RS | UHCI_CMD_MAXP | UHCI_CMD_CF);
    return 0;
}

static void uhci_stop(struct uhci_controller* ctrl)
{
    uhci_writew(ctrl, UHCI_CMD, 0);
}

static int uhci_port_reset(struct uhci_controller* ctrl, int port)
{
    u16 reg = (port == 0) ? UHCI_PORTSC1 : UHCI_PORTSC2;
    u16 val;
    int timeout;

    val = uhci_readw(ctrl, reg);
    val |= UHCI_PORT_PR;
    uhci_writew(ctrl, reg, val);

    for (timeout = 0; timeout < 50000; timeout++) {
        inb(0x80);
    }

    val = uhci_readw(ctrl, reg);
    val &= ~UHCI_PORT_PR;
    uhci_writew(ctrl, reg, val);

    for (timeout = 0; timeout < 50000; timeout++) {
        val = uhci_readw(ctrl, reg);
        if ((val & UHCI_PORT_PR) == 0 && (val & UHCI_PORT_PED))
            return 0;
        inb(0x80);
    }

    return -1;
}

static int uhci_port_status(struct uhci_controller* ctrl, int port)
{
    u16 reg = (port == 0) ? UHCI_PORTSC1 : UHCI_PORTSC2;
    return uhci_readw(ctrl, reg);
}

static u64 uhci_alloc_phys_page(void)
{
    u64 page = pmm_alloc_contiguous_low(1);
    if (page)
        Harlin_Fill((void*)(page + 0xFFFF800000000000ULL), 0, 4096);
    return page;
}

static void uhci_wait_td(struct uhci_controller* ctrl, struct uhci_td* td, int timeout_ms)
{
    volatile struct uhci_td* vtd = td;
    int i;

    (void)ctrl;
    for (i = 0; i < timeout_ms * 100; i++) {
        if (!(vtd->status & UHCI_TD_ACTIVE))
            break;
        inb(0x80);
    }
}

static int uhci_control_transfer(struct uhci_controller* ctrl,
    u8 dev_addr, u8 endpt, u8 max_pkt,
    struct usb_setup_pkt* setup, void* data, int data_len, int dir)
{
    u64 td_phys[3];
    struct uhci_td* td_virt[3];
    u64 data_buf_phys = 0;
    void* data_buf_virt = 0;
    int num_tds;
    int i;
    u16 status;
    int toggle_in, toggle_out;
    u64 qh_phys;
    struct uhci_qh* qh_virt;

    td_phys[0] = uhci_alloc_phys_page();
    td_phys[1] = uhci_alloc_phys_page();
    td_phys[2] = uhci_alloc_phys_page();
    if (!td_phys[0] || !td_phys[1] || !td_phys[2]) {
        if (td_phys[0]) pmm_free(td_phys[0]);
        if (td_phys[1]) pmm_free(td_phys[1]);
        if (td_phys[2]) pmm_free(td_phys[2]);
        return -1;
    }

    td_virt[0] = (struct uhci_td*)(td_phys[0] + 0xFFFF800000000000ULL);
    td_virt[1] = (struct uhci_td*)(td_phys[1] + 0xFFFF800000000000ULL);
    td_virt[2] = (struct uhci_td*)(td_phys[2] + 0xFFFF800000000000ULL);

    if (data_len > 0 && data) {
        data_buf_phys = uhci_alloc_phys_page();
        if (!data_buf_phys) {
            pmm_free(td_phys[0]);
            pmm_free(td_phys[1]);
            pmm_free(td_phys[2]);
            return -1;
        }
        data_buf_virt = (void*)(data_buf_phys + 0xFFFF800000000000ULL);
        if (dir == USB_DIR_OUT)
            Harlin_Copy(data_buf_virt, data, data_len);
        else
            Harlin_Fill(data_buf_virt, 0, data_len > 4096 ? 4096 : data_len);
    }

    num_tds = (data_len > 0 && data) ? 3 : 2;

    toggle_in = 1;
    toggle_out = 1;

    for (i = 0; i < 3; i++) {
        Harlin_Fill(td_virt[i], 0, sizeof(struct uhci_td));
    }

    td_virt[0]->link = UHCI_TD_LINK((u32)td_phys[1], 0, 0);
    td_virt[0]->status = UHCI_TD_ACTIVE | UHCI_TD_IOC;
    td_virt[0]->token = UHCI_TOKEN(dev_addr, endpt, USB_PID_SETUP, 0, max_pkt);
    td_virt[0]->buffer = (u32)td_phys[0] + 64;
    if (setup)
        Harlin_Copy((u8*)td_virt[0] + 64, setup, sizeof(struct usb_setup_pkt));

    if (num_tds == 3) {
        td_virt[1]->link = UHCI_TD_LINK((u32)td_phys[2], 0, 0);
        td_virt[1]->status = UHCI_TD_ACTIVE | UHCI_TD_IOC;
        if (dir == USB_DIR_IN) {
            td_virt[1]->token = UHCI_TOKEN(dev_addr, endpt, USB_PID_IN, toggle_in, max_pkt);
            td_virt[1]->buffer = (u32)data_buf_phys;
        } else {
            td_virt[1]->token = UHCI_TOKEN(dev_addr, endpt, USB_PID_OUT, toggle_out, max_pkt);
            td_virt[1]->buffer = (u32)data_buf_phys;
        }

        td_virt[2]->link = UHCI_TD_LINK(0, 0, 1);
        td_virt[2]->status = UHCI_TD_ACTIVE | UHCI_TD_IOC;
        if (dir == USB_DIR_IN) {
            td_virt[2]->token = UHCI_TOKEN(dev_addr, endpt, USB_PID_OUT, toggle_out, max_pkt);
        } else {
            td_virt[2]->token = UHCI_TOKEN(dev_addr, endpt, USB_PID_IN, toggle_in, max_pkt);
        }
        td_virt[2]->buffer = 0;
    } else {
        td_virt[1]->link = UHCI_TD_LINK(0, 0, 1);
        td_virt[1]->status = UHCI_TD_ACTIVE | UHCI_TD_IOC;
        if (dir == USB_DIR_IN) {
            td_virt[1]->token = UHCI_TOKEN(dev_addr, endpt, USB_PID_IN, toggle_in, max_pkt);
        } else {
            td_virt[1]->token = UHCI_TOKEN(dev_addr, endpt, USB_PID_OUT, toggle_out, max_pkt);
        }
        td_virt[1]->buffer = 0;
    }

    qh_phys = uhci_alloc_phys_page();
    if (!qh_phys) {
        pmm_free(td_phys[0]);
        pmm_free(td_phys[1]);
        pmm_free(td_phys[2]);
        if (data_buf_phys) pmm_free(data_buf_phys);
        return -1;
    }
    qh_virt = (struct uhci_qh*)(qh_phys + 0xFFFF800000000000ULL);
    Harlin_Fill(qh_virt, 0, sizeof(struct uhci_qh));
    qh_virt->link = UHCI_QH_LINK(0, 1);
    qh_virt->element = (u32)td_phys[0];

    ctrl->frame_list[0].ptr = UHCI_FRENTRY((u32)qh_phys, 0);

    uhci_wait_td(ctrl, td_virt[0], 1000);
    if (num_tds >= 2)
        uhci_wait_td(ctrl, td_virt[1], 1000);
    if (num_tds >= 3)
        uhci_wait_td(ctrl, td_virt[2], 1000);

    ctrl->frame_list[0].ptr = UHCI_FRENTRY(0, 1);

    status = td_virt[1]->status;
    if (num_tds >= 3)
        status |= td_virt[2]->status;

    if (dir == USB_DIR_IN && data_len > 0 && data && data_buf_virt) {
        u32 actual = td_virt[1]->status & 0x7FF;
        Harlin_Copy(data, data_buf_virt, actual < (u32)data_len ? actual : (u32)data_len);
    }

    pmm_free(qh_phys);
    pmm_free(td_phys[0]);
    pmm_free(td_phys[1]);
    pmm_free(td_phys[2]);
    if (data_buf_phys)
        pmm_free(data_buf_phys);

    if (status & (UHCI_TD_STALLED | UHCI_TD_CRC))
        return -1;

    if (num_tds >= 2 && (td_virt[1]->status & UHCI_TD_ACTIVE) && !(td_virt[1]->status & UHCI_TD_NAK))
        return -1;

    return 0;
}

static int uhci_get_device_desc(struct uhci_controller* ctrl,
    struct usb_device* dev, struct usb_device_desc* desc)
{
    struct usb_setup_pkt setup;

    Harlin_Fill(&setup, 0, sizeof(setup));
    setup.bmRequestType = 0x80;
    setup.bRequest = USB_REQ_GET_DESC;
    setup.wValue = (USB_DESC_DEVICE << 8) | 0;
    setup.wIndex = 0;
    setup.wLength = 8;

    if (uhci_control_transfer(ctrl, dev->address, 0, dev->max_packet_size,
        &setup, desc, 8, USB_DIR_IN) != 0)
        return -1;

    Harlin_Fill(&setup, 0, sizeof(setup));
    setup.bmRequestType = 0x80;
    setup.bRequest = USB_REQ_GET_DESC;
    setup.wValue = (USB_DESC_DEVICE << 8) | 0;
    setup.wIndex = 0;
    setup.wLength = 18;

    if (uhci_control_transfer(ctrl, dev->address, 0, dev->max_packet_size,
        &setup, desc, 18, USB_DIR_IN) != 0)
        return -1;

    return 0;
}

static int uhci_set_address(struct uhci_controller* ctrl,
    struct usb_device* dev)
{
    struct usb_setup_pkt setup;

    Harlin_Fill(&setup, 0, sizeof(setup));
    setup.bmRequestType = 0x00;
    setup.bRequest = USB_REQ_SET_ADDR;
    setup.wValue = dev->address;
    setup.wIndex = 0;
    setup.wLength = 0;

    return uhci_control_transfer(ctrl, 0, 0, dev->max_packet_size,
        &setup, 0, 0, USB_DIR_OUT);
}

static int uhci_set_configuration(struct uhci_controller* ctrl,
    struct usb_device* dev, u8 config)
{
    struct usb_setup_pkt setup;

    Harlin_Fill(&setup, 0, sizeof(setup));
    setup.bmRequestType = 0x00;
    setup.bRequest = USB_REQ_SET_CONFIG;
    setup.wValue = config;
    setup.wIndex = 0;
    setup.wLength = 0;

    return uhci_control_transfer(ctrl, dev->address, 0, dev->max_packet_size,
        &setup, 0, 0, USB_DIR_OUT);
}

static int uhci_enumerate_device(struct uhci_controller* ctrl, int port)
{
    struct usb_device* dev;
    struct usb_device_desc desc;
    int addr;

    if (device_count >= USB_MAX_DEVICES)
        return -1;

    dev = &devices[device_count];
    Harlin_Fill(dev, 0, sizeof(struct usb_device));

    dev->port = (u8)port;
    dev->address = 0;
    dev->max_packet_size = 8;

    addr = device_count + 1;
    if (addr > 127)
        return -1;

    if (uhci_get_device_desc(ctrl, dev, &desc) != 0)
        return -1;

    dev->max_packet_size = desc.bMaxPacketSize0;
    dev->address = (u8)addr;

    if (uhci_set_address(ctrl, dev) != 0)
        return -1;

    dev->vendor_id = desc.idVendor;
    dev->product_id = desc.idProduct;
    dev->class_code = desc.bDeviceClass;
    dev->subclass = desc.bDeviceSubClass;
    dev->protocol = desc.bDeviceProtocol;
    dev->controller_index = controller_count - 1;

    if (desc.bNumConfigurations > 0)
        uhci_set_configuration(ctrl, dev, 1);

    dev->present = 1;
    device_count++;
    return 0;
}

static int uhci_init_controller(struct pci_device* pcidev)
{
    struct uhci_controller* ctrl;
    u64 bar0;
    u64 fl_phys;
    int i;

    if (controller_count >= USB_MAX_CONTROLLERS)
        return -1;

    if (pci_enable_busmaster(pcidev) != 0)
        return -1;

    ctrl = &controllers[controller_count];
    Harlin_Fill(ctrl, 0, sizeof(struct uhci_controller));

    if (pci_get_bar(pcidev, 0, &bar0) != 0)
        return -1;
    ctrl->io_base = (u16)(bar0 & 0xFFFC);
    ctrl->irq = pcidev->irq_line;
    ctrl->bus = pcidev->bus;
    ctrl->dev = pcidev->device;
    ctrl->func = pcidev->func;

    if (uhci_reset(ctrl) != 0)
        return -1;

    fl_phys = uhci_alloc_phys_page();
    if (!fl_phys)
        return -1;
    ctrl->frame_list = (struct uhci_frentry*)(fl_phys + 0xFFFF800000000000ULL);
    ctrl->frame_list_phys = fl_phys;

    for (i = 0; i < UHCI_FRAME_LIST_SIZE; i++)
        ctrl->frame_list[i].ptr = UHCI_FRENTRY(0, 1);

    uhci_writel(ctrl, UHCI_FLBASEADD, (u32)fl_phys);
    uhci_writew(ctrl, UHCI_FRNUM, 0);
    uhci_writew(ctrl, UHCI_SOFMOD, 64);

    if (uhci_start(ctrl) != 0)
        return -1;

    ctrl->present = 1;
    controller_count++;

    for (i = 0; i < 2; i++) {
        u16 port_status;
        port_status = (i == 0) ? uhci_readw(ctrl, UHCI_PORTSC1) : uhci_readw(ctrl, UHCI_PORTSC2);
        if (port_status & UHCI_PORT_CCS) {
            u16 clear_bits = UHCI_PORT_CSC | UHCI_PORT_PEDC;
            if (i == 0)
                uhci_writew(ctrl, UHCI_PORTSC1, uhci_readw(ctrl, UHCI_PORTSC1) | clear_bits);
            else
                uhci_writew(ctrl, UHCI_PORTSC2, uhci_readw(ctrl, UHCI_PORTSC2) | clear_bits);

            if (uhci_port_reset(ctrl, i) == 0) {
                uhci_enumerate_device(ctrl, i);
            }
        }
    }

    return 0;
}

int usb_init(void)
{
    struct pci_device pcidev;
    int index;

    controller_count = 0;
    device_count = 0;

    index = 0;
    while (pci_find_class(USB_CLASS_UHCI, USB_SUBCLASS_UHCI, &pcidev, index) >= 0) {
        if (pcidev.prog_if == USB_PROGIF_UHCI) {
            if (uhci_init_controller(&pcidev) == 0) {
                screen_puts("[usb] uhci controller initialized\n");
            } else {
                screen_puts("[usb] uhci controller failed\n");
            }
        }
        index++;
    }

    if (controller_count == 0) {
        screen_puts("[usb] no uhci controller found\n");
    }

    return controller_count;
}

int usb_controller_count(void)
{
    return controller_count;
}

int usb_device_count(void)
{
    return device_count;
}

int usb_get_device(int index, struct usb_device* out)
{
    if (index < 0 || index >= device_count || !out)
        return -1;
    *out = devices[index];
    return 0;
}
