#ifndef E820_H
#define E820_H

#include "harlin_API.h"

#define E820_MAX_ENTRIES 64
#define E820_ADDR_SMAP    0x534D4150U
#define E820_INT          0x15

#define E820_TYPE_USABLE  1
#define E820_TYPE_RESERVED 2
#define E820_TYPE_ACPI    3
#define E820_TYPE_NVS     4
#define E820_TYPE_BAD     5

struct e820_entry {
    u64 base;
    u64 length;
    u32 type;
    u32 ext_attr;
} __attribute__((packed));

struct e820_map {
    u32 count;
    struct e820_entry entries[E820_MAX_ENTRIES];
    u64 total_usable_bytes;
    u64 top_usable_addr;
};

int e820_probe(struct e820_map* map);
void e820_dump(const struct e820_map* map);
u64 e820_total_usable(const struct e820_map* map);
u64 e820_top_usable(const struct e820_map* map);

#define Harlin_E820Probe             e820_probe
#define Harlin_E820Dump              e820_dump
#define Harlin_E820TotalUsable       e820_total_usable
#define Harlin_E820TopUsable         e820_top_usable

#endif
