#include "ahci.h"
#include "pci.h"
#include "pmm.h"
#include "vmm.h"
#include "io.h"
#include "spinlock.h"

static void ahci_memset(void* dst, int c, u64 n)
{
    u8* d = (u8*)dst;
    u64 i;
    for (i = 0; i < n; i++) d[i] = (u8)c;
}

static void ahci_memcpy(void* dst, const void* src, u64 n)
{
    u8* d = (u8*)dst;
    const u8* s = (const u8*)src;
    u64 i;
    for (i = 0; i < n; i++) d[i] = s[i];
}

#define memset ahci_memset
#define memcpy ahci_memcpy

static volatile struct ahci_hba* hba = 0;
static u64 hba_phys = 0;
static u32 hba_pi = 0;
static u32 hba_cap = 0;
static struct ahci_cmd_header* ahci_clb[AHCI_MAX_PORTS] = { 0 };
static struct ahci_received_fis* ahci_fb[AHCI_MAX_PORTS] = { 0 };
static struct ahci_cmd_table* ahci_ctba[AHCI_MAX_PORTS] = { 0 };
static u64 ahci_clb_phys[AHCI_MAX_PORTS] = { 0 };
static u64 ahci_fb_phys[AHCI_MAX_PORTS] = { 0 };
static u64 ahci_ctba_phys[AHCI_MAX_PORTS] = { 0 };
static u64 ahci_scratch_phys[AHCI_MAX_PORTS] = { 0 };
static void* ahci_scratch[AHCI_MAX_PORTS] = { 0 };
static struct spinlock ahci_lock = { 0 };
static int ahci_lock_inited = 0;
static int ahci_inited = 0;

#define MMIO_BASE 0xFFFF800040000000ULL

static u32 ahci_read(u64 off)
{
    return *(volatile u32*)((u64)hba + off);
}

static void ahci_write(u64 off, u32 val)
{
    *(volatile u32*)((u64)hba + off) = val;
}

static u32 ahci_port_read(u8 port, u64 off)
{
    volatile struct ahci_port* p = &hba->ports[port];
    return *(volatile u32*)((u64)p + off);
}

static void ahci_port_write(u8 port, u64 off, u32 val)
{
    volatile struct ahci_port* p = &hba->ports[port];
    *(volatile u32*)((u64)p + off) = val;
}

static u64 ahci_alloc_pages(u32 pages)
{
    u64 cur = 0;
    u32 i;
    for (i = 0; i < pages; i++) {
        u64 a = pmm_alloc();
        if (a == 0) return 0;
        if (i == 0) cur = a;
        else if (a != cur + i * 0x1000) {
            return 0;
        }
    }
    return cur;
}

static int ahci_map_mmio(u64 phys, u64 size, void** out_virt)
{
    u64 aligned = phys & ~0xFFFULL;
    u64 offset = phys - aligned;
    u64 total = (size + offset + 0xFFF) & ~0xFFFULL;
    u64 virt = MMIO_BASE + aligned;
    u64 i;
    for (i = 0; i < total; i += 0x1000) {
        vmm_map(virt + i, aligned + i, VMM_PRESENT | VMM_WRITABLE);
    }
    *out_virt = (void*)(virt + offset);
    return 0;
}

static void ahci_init_port(u8 port)
{
    u32 cmd = ahci_port_read(port, 0x18);
    cmd &= ~AHCI_PXCMD_ST;
    cmd &= ~AHCI_PXCMD_FRE;
    ahci_port_write(port, 0x18, cmd);

    while (ahci_port_read(port, 0x18) & (AHCI_PXCMD_CR | AHCI_PXCMD_FR)) {
    }

    ahci_port_write(port, 0x24, 0xFFFFFFFF);

    ahci_port_write(port, 0x00, (u32)ahci_clb_phys[port]);
    ahci_port_write(port, 0x04, (u32)(ahci_clb_phys[port] >> 32));

    ahci_port_write(port, 0x08, (u32)ahci_fb_phys[port]);
    ahci_port_write(port, 0x0C, (u32)(ahci_fb_phys[port] >> 32));

    ahci_port_write(port, 0x14, 0);
    ahci_port_write(port, 0x18, AHCI_PXCMD_ICC_ACTIVE | AHCI_PXCMD_POD | AHCI_PXCMD_SUD);
    ahci_port_write(port, 0x18, ahci_port_read(port, 0x18) | AHCI_PXCMD_FRE);
    ahci_port_write(port, 0x18, ahci_port_read(port, 0x18) | AHCI_PXCMD_ST);
}

static int ahci_port_detect(u8 port)
{
    u32 ssts = ahci_port_read(port, 0x28);
    u8 det = ssts & 0x0F;
    u8 ipm = (ssts >> 8) & 0x0F;
    if (det != 3) return 0;
    if (ipm != 1) return 0;
    return 1;
}

static int ahci_port_alloc(u8 port)
{
    u64 clb_phys;
    u64 fb_phys;
    u64 ctba_phys;
    u64 scratch_phys;
    void* scratch_virt;

    clb_phys = ahci_alloc_pages((AHCI_CLB_ENTRIES + 0xFFF) >> 12);
    if (clb_phys == 0) return -1;
    fb_phys = ahci_alloc_pages((AHCI_FB_ENTRIES + 0xFFF) >> 12);
    if (fb_phys == 0) return -1;
    ctba_phys = ahci_alloc_pages((AHCI_CTBA_ENTRIES + 0xFFF) >> 12);
    if (ctba_phys == 0) return -1;
    scratch_phys = pmm_alloc();
    if (scratch_phys == 0) return -1;

    ahci_clb_phys[port] = clb_phys;
    ahci_fb_phys[port] = fb_phys;
    ahci_ctba_phys[port] = ctba_phys;
    ahci_scratch_phys[port] = scratch_phys;

    if (ahci_map_mmio(clb_phys, AHCI_CLB_ENTRIES, (void**)&ahci_clb[port]) < 0) return -1;
    if (ahci_map_mmio(fb_phys, AHCI_FB_ENTRIES, (void**)&ahci_fb[port]) < 0) return -1;
    if (ahci_map_mmio(ctba_phys, AHCI_CTBA_ENTRIES, (void**)&ahci_ctba[port]) < 0) return -1;
    if (ahci_map_mmio(scratch_phys, 0x1000, &scratch_virt) < 0) return -1;
    ahci_scratch[port] = scratch_virt;

    return 0;
}

static int ahci_port_type(u8 port)
{
    u32 sig = ahci_port_read(port, 0x24);
    if (sig == 0xEB140101) return 1;
    if (sig == 0xC33C0101) return 2;
    if (sig == 0x96690101) return 3;
    return 0;
}

static int ahci_submit_io(u8 port, u8 slot, struct ahci_cmd_table* tbl, u32 sectors)
{
    volatile struct ahci_port* p = &hba->ports[port];
    int spin = 0;

    p->is = (u32)~0;

    tbl->prdt[0].dba_low = (u32)ahci_scratch_phys[port];
    tbl->prdt[0].dba_high = (u32)(ahci_scratch_phys[port] >> 32);
    tbl->prdt[0].byte_count = (sectors * 512) - 1;
    tbl->prdt[0].flags = AHCI_PRDT_FLAG_LAST;

    ahci_clb[port][slot].flags = 0x05;
    ahci_clb[port][slot].prdtl = 1;
    ahci_clb[port][slot].prdbc = 0;
    ahci_clb[port][slot].ctba_low = (u32)ahci_ctba_phys[port];
    ahci_clb[port][slot].ctba_upper = (u32)(ahci_ctba_phys[port] >> 32);

    p->ci = p->ci | (1 << slot);

    while ((p->ci & (1 << slot)) != 0) {
        if (p->is & AHCI_PXIS_TFES) return -1;
        if (++spin > 10000000) return -1;
    }

    return 0;
}

static int ahci_do_rw(struct ahci_device* dev, u64 lba, u8 count, void* buf, int write)
{
    u8 port = dev->port;
    u64 flags;
    u8 slot = 0;
    struct ahci_cmd_table* tbl = (struct ahci_cmd_table*)ahci_ctba[port];
    u32 sectors = (u32)count;
    u8* byte_buf = (u8*)buf;
    u32 i;
    int rc;

    if (!ahci_lock_inited) {
        spinlock_init(&ahci_lock);
        ahci_lock_inited = 1;
    }
    flags = spinlock_acquire_irqsave(&ahci_lock);

    for (i = 0; i < sectors; i++) {
        struct ahci_fis_h2d* cfis;
        u8* scratch;

        memset(tbl, 0, sizeof(struct ahci_cmd_table));

        cfis = (struct ahci_fis_h2d*)tbl->cfis;
        cfis->type = AHCI_FIS_TYPE_H2D;
        cfis->flags = 0x80;
        cfis->command = write ? AHCI_CMD_WRITE_DMA : AHCI_CMD_READ_DMA;
        cfis->feature_low = 0;
        cfis->lba_low = (u8)((lba + i) & 0xFF);
        cfis->lba_mid = (u8)(((lba + i) >> 8) & 0xFF);
        cfis->lba_high = (u8)(((lba + i) >> 16) & 0xFF);
        cfis->device = 0x40;
        cfis->lba_low_exp = (u8)(((lba + i) >> 24) & 0xFF);
        cfis->lba_mid_exp = (u8)(((lba + i) >> 32) & 0xFF);
        cfis->lba_high_exp = (u8)(((lba + i) >> 40) & 0xFF);
        cfis->sector_count_low = 1;
        cfis->sector_count_high = 0;

        scratch = (u8*)ahci_scratch[port];
        if (write) {
            memcpy(scratch, byte_buf + i * 512, 512);
        }

        rc = ahci_submit_io(port, slot, tbl, 1);
        if (rc < 0) {
            spinlock_release_irqrestore(&ahci_lock, flags);
            return rc;
        }

        if (!write) {
            memcpy(byte_buf + i * 512, scratch, 512);
        }
    }

    spinlock_release_irqrestore(&ahci_lock, flags);
    return 0;
}

int ahci_init(void)
{
    struct pci_device dev;
    int idx = 0;
    u32 ghc;
    u32 cap;
    u32 pi;
    u32 np;
    u8 port;

    if (ahci_inited) return 0;

    if (pci_find_class(AHCI_CLASS_SATA, AHCI_SUBCLASS_AHCI, &dev, idx) < 0) {
        return -1;
    }

    pci_enable_busmaster(&dev);

    hba_phys = dev.bar[5];
    if (hba_phys == 0) {
        return -1;
    }

    if (ahci_map_mmio(hba_phys, sizeof(struct ahci_hba), (void**)&hba) < 0) {
        return -1;
    }

    ghc = ahci_read(0x04);
    if (!(ghc & 0x80000000)) {
        ahci_write(0x04, ghc | 0x80000000);
    }

    cap = ahci_read(0x00);
    pi = ahci_read(0x0C);
    np = ((cap & 0x1F0000) >> 16) + 1;
    hba_cap = cap;
    hba_pi = pi;

    for (port = 0; port < AHCI_MAX_PORTS && port < np; port++) {
        if (!(pi & (1 << port))) continue;
        if (!ahci_port_detect(port)) continue;
        if (ahci_port_alloc(port) < 0) continue;
        ahci_init_port(port);
    }

    ahci_inited = 1;
    return 0;
}

int ahci_find_disk(struct ahci_device* out)
{
    u8 port;
    for (port = 0; port < AHCI_MAX_PORTS; port++) {
        if (!(hba_pi & (1 << port))) continue;
        if (!ahci_port_detect(port)) continue;
        if (ahci_port_type(port) == 1) {
            out->present = 1;
            out->port = port;
            out->atapi = 0;
            out->sig = ahci_port_read(port, 0x24);
            out->clb = ahci_clb[port];
            out->fb = ahci_fb[port];
            out->ctba = ahci_ctba[port];
            out->scratch = ahci_scratch[port];
            out->scratch_phys = ahci_scratch_phys[port];
            out->sectors = 0;
            return 0;
        }
    }
    return -1;
}

int ahci_identify(struct ahci_device* dev)
{
    struct ahci_cmd_table* tbl = (struct ahci_cmd_table*)ahci_ctba[dev->port];
    struct ahci_fis_h2d* cfis;
    u64 flags;
    u8 slot = 0;
    u16* id;
    volatile struct ahci_port* p = &hba->ports[dev->port];
    int spin = 0;

    if (!ahci_lock_inited) {
        spinlock_init(&ahci_lock);
        ahci_lock_inited = 1;
    }
    flags = spinlock_acquire_irqsave(&ahci_lock);

    memset(tbl, 0, sizeof(struct ahci_cmd_table));

    cfis = (struct ahci_fis_h2d*)tbl->cfis;
    cfis->type = AHCI_FIS_TYPE_H2D;
    cfis->flags = 0x80;
    cfis->command = AHCI_CMD_IDENTIFY;
    cfis->device = 0;

    tbl->prdt[0].dba_low = (u32)dev->scratch_phys;
    tbl->prdt[0].dba_high = (u32)(dev->scratch_phys >> 32);
    tbl->prdt[0].byte_count = 511;
    tbl->prdt[0].flags = AHCI_PRDT_FLAG_LAST;

    dev->clb[slot].flags = 0x05;
    dev->clb[slot].prdtl = 1;
    dev->clb[slot].prdbc = 0;
    dev->clb[slot].ctba_low = (u32)ahci_ctba_phys[dev->port];
    dev->clb[slot].ctba_upper = (u32)(ahci_ctba_phys[dev->port] >> 32);

    p->is = (u32)~0;
    p->ci = 1 << slot;

    while ((p->ci & (1 << slot)) != 0) {
        if (p->is & AHCI_PXIS_TFES) {
            spinlock_release_irqrestore(&ahci_lock, flags);
            return -1;
        }
        if (++spin > 10000000) {
            spinlock_release_irqrestore(&ahci_lock, flags);
            return -1;
        }
    }

    id = (u16*)dev->scratch;
    dev->sectors = ((u64)id[61] << 16) | id[60];

    spinlock_release_irqrestore(&ahci_lock, flags);
    return 0;
}

int ahci_read_sectors(struct ahci_device* dev, u64 lba, u8 count, void* buf)
{
    return ahci_do_rw(dev, lba, count, buf, 0);
}

int ahci_write_sectors(struct ahci_device* dev, u64 lba, u8 count, const void* buf)
{
    return ahci_do_rw(dev, lba, count, (void*)buf, 1);
}
