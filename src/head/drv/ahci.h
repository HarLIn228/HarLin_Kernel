#ifndef AHCI_H
#define AHCI_H

#include "harlin_API.h"

#define AHCI_MAX_PORTS        32
#define AHCI_MAX_CMD_SLOTS    32
#define AHCI_PRDT_ENTRIES     8
#define AHCI_CLB_ENTRIES      (AHCI_MAX_CMD_SLOTS * sizeof(struct ahci_cmd_header))
#define AHCI_FB_ENTRIES       (256)
#define AHCI_CTBA_ENTRIES     (AHCI_MAX_CMD_SLOTS * sizeof(struct ahci_cmd_table))
#define AHCI_SECTOR_SIZE      512

#define AHCI_CLASS_SATA       0x01
#define AHCI_SUBCLASS_AHCI    0x06

#define AHCI_FIS_TYPE_H2D     0x27
#define AHCI_FIS_TYPE_D2H     0x34
#define AHCI_FIS_TYPE_DMA_ACT 0x39
#define AHCI_FIS_TYPE_DMA_SET 0x41
#define AHCI_FIS_TYPE_BIST    0x58
#define AHCI_FIS_TYPE_PIO_SET 0x5F
#define AHCI_FIS_TYPE_DEV_BIT 0xA1

#define AHCI_CMD_READ_DMA     0x25
#define AHCI_CMD_WRITE_DMA    0x35
#define AHCI_CMD_IDENTIFY     0xEC

#define AHCI_CMD_SLOT_MASK    0x1F
#define AHCI_PXCMD_ST         0x0001
#define AHCI_PXCMD_CR         0x0008
#define AHCI_PXCMD_FR         0x0010
#define AHCI_PXCMD_FRE        0x0010
#define AHCI_PXCMD_SUD        0x0100
#define AHCI_PXCMD_POD        0x0200
#define AHCI_PXCMD_ICC_ACTIVE 0x10000000

#define AHCI_PXIS_TFES        0x40000000

#define AHCI_TFD_STS_BSY      0x80
#define AHCI_TFD_STS_DRQ      0x08
#define AHCI_TFD_STS_ERR      0x01

#define AHCI_CAP_NP_MASK      0x1F
#define AHCI_CAP_S64A         0x80000000
#define AHCI_CAP_SNCQ         0x40000000

#define AHCI_PRDT_FLAG_LAST   (1 << 15)

struct ahci_fis_h2d {
    u8  type;
    u8  flags;
    u8  command;
    u8  feature_low;
    u8  lba_low;
    u8  lba_mid;
    u8  lba_high;
    u8  device;
    u8  lba_low_exp;
    u8  lba_mid_exp;
    u8  lba_high_exp;
    u8  feature_high;
    u8  sector_count_low;
    u8  sector_count_high;
    u8  icc;
    u8  control;
    u8  rsvd[4];
} __attribute__((packed));

struct ahci_fis_d2h {
    u8  type;
    u8  flags;
    u8  status;
    u8  error;
    u8  lba_low;
    u8  lba_mid;
    u8  lba_high;
    u8  device;
    u8  lba_low_exp;
    u8  lba_mid_exp;
    u8  lba_high_exp;
    u8  rsvd;
    u8  sector_count_low;
    u8  sector_count_high;
    u8  rsvd2[2];
    u8  icc;
    u8  rsvd3[4];
} __attribute__((packed));

struct ahci_prdt_entry {
    u32 dba_low;
    u32 dba_high;
    u32 flags;
    u32 byte_count;
} __attribute__((packed));

struct ahci_cmd_header {
    u8  flags;
    u8  prdtl;
    u16 prdbc;
    u32 ctba_low;
    u32 ctba_upper;
    u32 rsvd[4];
} __attribute__((packed));

struct ahci_cmd_table {
    u8  cfis[64];
    u8  acmd[16];
    u8  rsvd[48];
    struct ahci_prdt_entry prdt[AHCI_PRDT_ENTRIES];
} __attribute__((packed));

struct ahci_received_fis {
    u8  dsafis[0x1C];
    u8  psafis[0x14];
    u8  rfis[0x14];
    u8  unknown[0x40];
    u8  sdbfis[0x08];
    u8  ufis[0x40];
} __attribute__((packed));

struct ahci_port {
    u32 clb_low;
    u32 clb_upper;
    u32 fb_low;
    u32 fb_upper;
    u32 is;
    u32 ie;
    u32 cmd;
    u32 rsvd0;
    u32 tfd;
    u32 sig;
    u32 ssts;
    u32 sctl;
    u32 serr;
    u32 sact;
    u32 ci;
    u32 sntf;
    u32 fbs;
    u32 rsvd1[11];
    u32 vendor[4];
} __attribute__((packed));

struct ahci_hba {
    u32 cap;
    u32 ghc;
    u32 is;
    u32 pi;
    u32 vs;
    u32 ccc_ctl;
    u32 ccc_ports;
    u32 em_loc;
    u32 em_ctl;
    u32 cap2;
    u32 bohc;
    u8  rsvd[0xA0 - 0x2C];
    struct ahci_port ports[AHCI_MAX_PORTS];
} __attribute__((packed));

struct ahci_device {
    u8  present;
    u8  port;
    u8  atapi;
    u32 sig;
    u64 sectors;
    struct ahci_cmd_header* clb;
    struct ahci_received_fis* fb;
    struct ahci_cmd_table* ctba;
    void* scratch;
    u64 scratch_phys;
};

int ahci_init(void);
int ahci_find_disk(struct ahci_device* out);
int ahci_read_sectors(struct ahci_device* dev, u64 lba, u8 count, void* buf);
int ahci_write_sectors(struct ahci_device* dev, u64 lba, u8 count, const void* buf);
int ahci_identify(struct ahci_device* dev);

#define Harlin_AhciInit              ahci_init
#define Harlin_AhciFindDisk          ahci_find_disk
#define Harlin_AhciReadSectors       ahci_read_sectors
#define Harlin_AhciWriteSectors      ahci_write_sectors
#define Harlin_AhciIdentify          ahci_identify

#endif
