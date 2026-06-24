#ifndef ACPI_H
#define ACPI_H

#include "harlin_API.h"

struct acpi_sdt_header {
    u8  signature[4];
    u32 length;
    u8  revision;
    u8  checksum;
    u8  oem_id[6];
    u8  oem_table_id[8];
    u32 oem_revision;
    u32 creator_id;
    u32 creator_revision;
} __attribute__((packed));

struct acpi_rsdp {
    u8  signature[8];
    u8  checksum;
    u8  oem_id[6];
    u8  revision;
    u32 rsdt_addr;
    u32 length;
    u64 xsdt_addr;
    u8  ext_checksum;
    u8  reserved[3];
} __attribute__((packed));

struct acpi_xsdt {
    struct acpi_sdt_header header;
    u64 entries[];
} __attribute__((packed));

struct acpi_gas {
    u8  space_id;
    u8  bit_width;
    u8  bit_offset;
    u8  access_size;
    u64 address;
} __attribute__((packed));

struct acpi_fadt {
    struct acpi_sdt_header header;
    u32 firmware_ctrl;
    u32 dsdt;
    u8  reserved1;
    u8  preferred_pm_profile;
    u16 sci_int;
    u32 smi_cmd;
    u8  acpi_enable;
    u8  acpi_disable;
    u8  s4bios_req;
    u8  pstate_cnt;
    u32 pm1a_evt_blk;
    u32 pm1b_evt_blk;
    u32 pm1a_cnt_blk;
    u32 pm1b_cnt_blk;
    u32 pm2_cnt_blk;
    u32 pm_tmr_blk;
    u32 gpe0_blk;
    u32 gpe1_blk;
    u8  pm1_evt_len;
    u8  pm1_cnt_len;
    u8  pm2_cnt_len;
    u8  pm_tmr_len;
    u8  gpe0_len;
    u8  gpe1_len;
    u8  gpe1_base;
    u8  cst_cnt;
    u16 p_lvl2_lat;
    u16 p_lvl3_lat;
    u16 flush_size;
    u16 flush_stride;
    u8  duty_offset;
    u8  duty_width;
    u8  day_alrm;
    u8  mon_alrm;
    u8  century;
    u16 iapc_boot_arch;
    u8  reserved2;
    u32 flags;
    struct acpi_gas reset_reg;
    u8  reset_value;
    u8  reserved3[3];
    u64 x_firmware_ctrl;
    u64 x_dsdt;
    struct acpi_gas x_pm1a_evt_blk;
    struct acpi_gas x_pm1b_evt_blk;
    struct acpi_gas x_pm1a_cnt_blk;
    struct acpi_gas x_pm1b_cnt_blk;
    struct acpi_gas x_pm2_cnt_blk;
    struct acpi_gas x_pm_tmr_blk;
    struct acpi_gas x_gpe0_blk;
    struct acpi_gas x_gpe1_blk;
} __attribute__((packed));

int  acpi_init(void);
void acpi_power_off(void);
void acpi_reboot(void);

#define Harlin_AcpiInit               acpi_init
#define Harlin_AcpiPowerOff           acpi_power_off
#define Harlin_AcpiReboot             acpi_reboot

#endif
