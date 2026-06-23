#include "e820.h"

int e820_probe(struct e820_map* map)
{
    if (!map)
        return -1;
    map->count = 0;
    map->total_usable_bytes = 0;
    map->top_usable_addr = 0;
    return -1;
}

void e820_dump(const struct e820_map* map)
{
    (void)map;
}

u64 e820_total_usable(const struct e820_map* map)
{
    if (!map) return 0;
    return map->total_usable_bytes;
}

u64 e820_top_usable(const struct e820_map* map)
{
    if (!map) return 0;
    return map->top_usable_addr;
}
