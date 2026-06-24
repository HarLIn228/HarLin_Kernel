#ifndef PCI_H
#define PCI_H

#include "harlin_API.h"

#define PCI_ADDR_PORT 0xCF8
#define PCI_DATA_PORT 0xCFC

#define PCI_MAX_BUS 256
#define PCI_MAX_DEV 32
#define PCI_MAX_FUNC 8

#define PCI_ECAM_MAX_BUS 256
#define PCI_CFG_SPACE_SIZE 256
#define PCI_ECAM_CFG_SIZE 4096

struct pci_device {
    u8  bus;
    u8  device;
    u8  func;
    u8  header_type;
    u16 vendor_id;
    u16 device_id;
    u8  class_code;
    u8  subclass;
    u8  prog_if;
    u8  irq_line;
    u32 bar[6];
};

int  pci_init(void);
int  pci_init_with_ecam(u64 ecam_base, u16 bus_start, u16 bus_end);
int  pci_ecam_available(void);
u64  pci_ecam_base(void);
u16  pci_ecam_bus_start(void);
u16  pci_ecam_bus_end(void);
int  pci_find_device(u16 vendor_id, u16 device_id, struct pci_device* out);
int  pci_find_class(u8 class_code, u8 subclass, struct pci_device* out, int index);
u32  pci_read(u8 bus, u8 dev, u8 func, u8 off);
void pci_write(u8 bus, u8 dev, u8 func, u8 off, u32 val);
u32  pci_read_ext(u8 bus, u8 dev, u8 func, u16 off);
void pci_write_ext(u8 bus, u8 dev, u8 func, u16 off, u32 val);
int  pci_enable_busmaster(struct pci_device* dev);
int  pci_get_bar(struct pci_device* dev, int bar_index, u64* out_addr);
int  pci_device_count(void);
int  pci_get_device(int index, struct pci_device* out);

#define Harlin_PciInit                pci_init
#define Harlin_PciInitWithEcam        pci_init_with_ecam
#define Harlin_PciEcamAvailable       pci_ecam_available
#define Harlin_PciEcamBase            pci_ecam_base
#define Harlin_PciEcamBusStart        pci_ecam_bus_start
#define Harlin_PciEcamBusEnd          pci_ecam_bus_end
#define Harlin_PciFindDevice          pci_find_device
#define Harlin_PciFindClass           pci_find_class
#define Harlin_PciRead                pci_read
#define Harlin_PciWrite               pci_write
#define Harlin_PciReadExt             pci_read_ext
#define Harlin_PciWriteExt            pci_write_ext
#define Harlin_PciEnableBusmaster     pci_enable_busmaster
#define Harlin_PciGetBar              pci_get_bar
#define Harlin_PciDeviceCount         pci_device_count
#define Harlin_PciGetDevice           pci_get_device

#endif
