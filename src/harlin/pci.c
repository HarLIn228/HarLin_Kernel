#include "pci.h"
#include "io.h"

#define PCI_MAX_DEVICES 64

static struct pci_device pci_devices[PCI_MAX_DEVICES];
static int pci_device_count_value = 0;

u32 pci_read(u8 bus, u8 dev, u8 func, u8 off)
{
    u32 addr = 0x80000000U
             | ((u32)bus << 16)
             | ((u32)dev << 11)
             | ((u32)func << 8)
             | (off & 0xFC);
    outl(PCI_ADDR_PORT, addr);
    return inl(PCI_DATA_PORT);
}

void pci_write(u8 bus, u8 dev, u8 func, u8 off, u32 val)
{
    u32 addr = 0x80000000U
             | ((u32)bus << 16)
             | ((u32)dev << 11)
             | ((u32)func << 8)
             | (off & 0xFC);
    outl(PCI_ADDR_PORT, addr);
    outl(PCI_DATA_PORT, val);
}

static void pci_scan_device(u8 bus, u8 dev, u8 func, u8 header_type)
{
    struct pci_device* d;
    u32 id;
    u32 reg2;
    int i;

    if (pci_device_count_value >= PCI_MAX_DEVICES)
        return;

    {
        int di;
        for (di = 0; di < pci_device_count_value; di++) {
            if (pci_devices[di].bus == bus &&
                pci_devices[di].device == dev &&
                pci_devices[di].func == func) {
                return;
            }
        }
    }

    id = pci_read(bus, dev, func, 0);
    if (id == 0xFFFFFFFFU || id == 0)
        return;
    if (!(id & 0xFFFF))
        return;
    if (!(id >> 16))
        return;

    d = &pci_devices[pci_device_count_value];
    d->bus = bus;
    d->device = dev;
    d->func = func;
    d->header_type = header_type;
    d->vendor_id = (u16)(id & 0xFFFF);
    d->device_id = (u16)(id >> 16);

    reg2 = pci_read(bus, dev, func, 0x08);
    d->class_code = (u8)(reg2 >> 24);
    d->subclass = (u8)((reg2 >> 16) & 0xFF);
    d->prog_if = (u8)((reg2 >> 8) & 0xFF);

    {
        u32 irq = pci_read(bus, dev, func, 0x3C);
        d->irq_line = (u8)(irq & 0xFF);
    }

    for (i = 0; i < 6; i++) {
        d->bar[i] = pci_read(bus, dev, func, 0x10 + (u8)(i * 4));
    }

    pci_device_count_value++;
}

static void pci_scan_bus(u8 bus);

static int pci_scan_bridge(u8 bus, u8 dev, u8 func)
{
    u32 reg18;
    u32 reg19;
    u8 secondary;
    u8 subordinate;
    int b;
    reg18 = pci_read(bus, dev, func, 0x18);
    reg19 = pci_read(bus, dev, func, 0x19);
    secondary = (u8)((reg18 >> 8) & 0xFF);
    subordinate = (u8)((reg19 >> 0) & 0xFF);
    if (secondary == 0 || subordinate == 0)
        return 0;
    if (secondary > subordinate)
        return 0;
    for (b = (int)secondary; b <= (int)subordinate; b++) {
        pci_scan_bus((u8)b);
    }
    return 1;
}

static void pci_scan_bus(u8 bus)
{
    u8 dev;
    u8 func;
    u8 max_func;
    u32 reg3;
    u8 header_type;

    for (dev = 0; dev < 32; dev++) {
        u32 id = pci_read(bus, dev, 0, 0);
        if (id == 0xFFFFFFFFU || id == 0)
            continue;
        reg3 = pci_read(bus, dev, 0, 0x0C);
        header_type = (u8)((reg3 >> 16) & 0xFF);
        max_func = (header_type & 0x80) ? 8 : 1;
        for (func = 0; func < max_func; func++) {
            u8 this_type;
            u32 this_reg3;
            this_reg3 = pci_read(bus, dev, func, 0x0C);
            this_type = (u8)((this_reg3 >> 16) & 0xFF);
            if (pci_device_count_value >= PCI_MAX_DEVICES)
                return;
            if (this_type == 0x01) {
                pci_scan_device(bus, dev, func, this_type);
                pci_scan_bridge(bus, dev, func);
            } else {
                pci_scan_device(bus, dev, func, this_type);
            }
        }
    }
}

int pci_init(void)
{
    u32 reg3;
    u8 header_type;
    u16 reg0_check;
    int i;

    pci_device_count_value = 0;
    for (i = 0; i < PCI_MAX_DEVICES; i++) {
        pci_devices[i].vendor_id = 0;
        pci_devices[i].device_id = 0;
    }

    reg0_check = (u16)(pci_read(0, 0, 0, 0) & 0xFFFF);
    if (reg0_check == 0xFFFF) {
        return -1;
    }

    reg3 = pci_read(0, 0, 0, 0x0C);
    header_type = (u8)((reg3 >> 16) & 0xFF);
    if (header_type & 0x80) {
        u8 func;
        for (func = 0; func < 8; func++) {
            u32 id = pci_read(0, 0, func, 0);
            if (id != 0xFFFFFFFFU && id != 0)
                pci_scan_bus(func);
        }
    } else {
        pci_scan_bus(0);
    }

    return pci_device_count_value;
}

int pci_device_count(void)
{
    return pci_device_count_value;
}

int pci_get_device(int index, struct pci_device* out)
{
    if (index < 0 || index >= pci_device_count_value || !out)
        return -1;
    *out = pci_devices[index];
    return 0;
}

int pci_find_device(u16 vendor_id, u16 device_id, struct pci_device* out)
{
    int i;
    for (i = 0; i < pci_device_count_value; i++) {
        if (pci_devices[i].vendor_id == vendor_id &&
            pci_devices[i].device_id == device_id) {
            if (out)
                *out = pci_devices[i];
            return i;
        }
    }
    return -1;
}

int pci_find_class(u8 class_code, u8 subclass, struct pci_device* out, int index)
{
    int i;
    int found = 0;
    for (i = 0; i < pci_device_count_value; i++) {
        if (pci_devices[i].class_code == class_code &&
            pci_devices[i].subclass == subclass) {
            if (found == index) {
                if (out)
                    *out = pci_devices[i];
                return i;
            }
            found++;
        }
    }
    return -1;
}

int pci_enable_busmaster(struct pci_device* dev)
{
    u32 cmd;
    if (!dev)
        return -1;
    cmd = pci_read(dev->bus, dev->device, dev->func, 0x04);
    cmd |= 0x07;
    pci_write(dev->bus, dev->device, dev->func, 0x04, cmd);
    return 0;
}

int pci_get_bar(struct pci_device* dev, int bar_index, u64* out_addr)
{
    u32 bar;
    u8 type;
    if (!dev || bar_index < 0 || bar_index >= 6 || !out_addr)
        return -1;
    bar = dev->bar[bar_index];
    if (bar & 0x01) {
        *out_addr = (u64)(bar & 0xFFFFFFFC);
        return 0;
    }
    type = (u8)((bar >> 1) & 0x03);
    if (type == 0x00) {
        *out_addr = (u64)(bar & 0xFFFFFFF0);
    } else if (type == 0x02) {
        if (bar_index + 1 >= 6)
            return -1;
        {
            u32 bar_hi = dev->bar[bar_index + 1];
            *out_addr = ((u64)bar_hi << 32) | (u64)(bar & 0xFFFFFFF0);
        }
    } else {
        return -1;
    }
    return 0;
}
