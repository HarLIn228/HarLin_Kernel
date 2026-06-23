#include "drv_loader.h"
#include "screen.h"
#include "io.h"
#include "harlin_API.h"

static struct drv_slot g_slots[DRV_SUBSYS_MAX][DRV_PROVIDERS_MAX];
static struct drv_provider g_providers[DRV_PROVIDERS_MAX];
static int g_inited = 0;

const char* drv_subsys_name(enum drv_subsys s)
{
    switch (s) {
        case DRV_SUBSYS_MEDIA:       return "media";
        case DRV_SUBSYS_LINK:        return "link";
        case DRV_SUBSYS_USB:         return "usb";
        case DRV_SUBSYS_SCREEN:      return "screen";
        case DRV_SUBSYS_SOUND:       return "sound";
        case DRV_SUBSYS_INPUT:       return "input";
        case DRV_SUBSYS_CLOCK:       return "clock";
        case DRV_SUBSYS_POWER:       return "power";
        case DRV_SUBSYS_BRIDGE:      return "bridge";
        case DRV_SUBSYS_SERIO:       return "serio";
        case DRV_SUBSYS_TTY:         return "tty";
        case DRV_SUBSYS_RTC:         return "rtc";
        case DRV_SUBSYS_GUARD:       return "guard";
        case DRV_SUBSYS_COOL:        return "cool";
        case DRV_SUBSYS_PIN:         return "pin";
        case DRV_SUBSYS_CIPHER:      return "cipher";
        case DRV_SUBSYS_BLOCK:       return "block";
        case DRV_SUBSYS_HMI:         return "hmi";
        case DRV_SUBSYS_FS:          return "fs";
        case DRV_SUBSYS_AGGREGATE:   return "aggregate";
        case DRV_SUBSYS_PULSE:       return "pulse";
        case DRV_SUBSYS_WAVE:        return "wave";
        case DRV_SUBSYS_SENSE:       return "sense";
        case DRV_SUBSYS_BOARD:       return "board";
        case DRV_SUBSYS_SCSI:        return "scsi";
        case DRV_SUBSYS_FLASH:       return "flash";
        case DRV_SUBSYS_VIRT:        return "virt";
        case DRV_SUBSYS_GPU:         return "gpu";
        default:                     return "reserved";
    }
}

static const char* subsys_name(enum drv_subsys s)
{
    return drv_subsys_name(s);
}

void drv_loader_init(void)
{
    int i, j;
    if (g_inited) return;
    for (i = 0; i < DRV_SUBSYS_MAX; i++) {
        for (j = 0; j < DRV_PROVIDERS_MAX; j++) {
            g_slots[i][j].ops = 0;
            g_slots[i][j].ctx = 0;
            g_slots[i][j].active = 0;
        }
    }
    for (i = 0; i < DRV_PROVIDERS_MAX; i++) {
        g_providers[i].name = 0;
        g_providers[i].scan = 0;
        g_providers[i].available = 0;
    }
    g_inited = 1;
}

int drv_register(const struct drv_ops* ops, void* ctx)
{
    int i;
    if (!ops || !ops->name) return -1;
    if ((int)ops->subsys < 0 || (int)ops->subsys >= DRV_SUBSYS_MAX) return -1;
    for (i = 0; i < DRV_PROVIDERS_MAX; i++) {
        if (g_slots[ops->subsys][i].ops == 0) {
            g_slots[ops->subsys][i].ops = ops;
            g_slots[ops->subsys][i].ctx = ctx;
            g_slots[ops->subsys][i].active = 0;
            return 0;
        }
    }
    return -1;
}

int drv_unregister(const char* name)
{
    int s, i;
    if (!name) return -1;
    for (s = 0; s < DRV_SUBSYS_MAX; s++) {
        for (i = 0; i < DRV_PROVIDERS_MAX; i++) {
            struct drv_slot* sl = &g_slots[s][i];
            if (sl->ops && sl->ops->name &&
                Harlin_Compare(sl->ops->name, name) == 0) {
                if (sl->active && sl->ops->remove) {
                    sl->ops->remove(sl->ctx);
                }
                sl->ops = 0;
                sl->ctx = 0;
                sl->active = 0;
                return 0;
            }
        }
    }
    return -1;
}

int drv_provider_register(const struct drv_provider* p)
{
    int i;
    if (!p || !p->name) return -1;
    for (i = 0; i < DRV_PROVIDERS_MAX; i++) {
        if (g_providers[i].name == 0) {
            g_providers[i].name = p->name;
            g_providers[i].scan = p->scan;
            g_providers[i].available = 0;
            return 0;
        }
    }
    return -1;
}

int drv_provider_scan_all(void)
{
    int i, found = 0;
    for (i = 0; i < DRV_PROVIDERS_MAX; i++) {
        if (g_providers[i].name && g_providers[i].scan) {
            g_providers[i].available = g_providers[i].scan();
            if (g_providers[i].available) found++;
        }
    }
    return found;
}

int drv_provider_available(const char* name)
{
    int i;
    if (!name) return 0;
    for (i = 0; i < DRV_PROVIDERS_MAX; i++) {
        if (g_providers[i].name &&
            Harlin_Compare(g_providers[i].name, name) == 0) {
            return g_providers[i].available;
        }
    }
    return 0;
}

int drv_load_subsys(enum drv_subsys s)
{
    int i, picked = 0;
    if ((int)s < 0 || (int)s >= DRV_SUBSYS_MAX) return -1;
    for (i = 0; i < DRV_PROVIDERS_MAX; i++) {
        struct drv_slot* sl = &g_slots[s][i];
        if (!sl->ops) continue;
        if (sl->ops->probe && sl->ops->probe(sl->ctx) != 0) continue;
        if (sl->ops->init && sl->ops->init(sl->ctx) != 0) {
            if (sl->ops->remove) sl->ops->remove(sl->ctx);
            continue;
        }
        sl->active = 1;
        picked++;
    }
    if (picked == 0) {
        switch (s) {
            case DRV_SUBSYS_MEDIA:  drv_fallback_media();  break;
            case DRV_SUBSYS_LINK:   drv_fallback_link();   break;
            case DRV_SUBSYS_SCREEN: drv_fallback_screen(); break;
            case DRV_SUBSYS_INPUT:  drv_fallback_input();  break;
            case DRV_SUBSYS_SOUND:  drv_fallback_sound();  break;
            case DRV_SUBSYS_RTC:    drv_fallback_rtc();    break;
            case DRV_SUBSYS_GUARD:  drv_fallback_guard();  break;
            case DRV_SUBSYS_BRIDGE: drv_fallback_bridge(); break;
            default: break;
        }
    }
    return picked;
}

int drv_load_all(void)
{
    int s, total = 0;
    if (!g_inited) drv_loader_init();
    drv_provider_scan_all();
    for (s = 0; s < DRV_SUBSYS_MAX; s++) {
        total += drv_load_subsys((enum drv_subsys)s);
    }
    return total;
}

int drv_active_count(enum drv_subsys s)
{
    int i, c = 0;
    if ((int)s < 0 || (int)s >= DRV_SUBSYS_MAX) return 0;
    for (i = 0; i < DRV_PROVIDERS_MAX; i++) {
        if (g_slots[s][i].active) c++;
    }
    return c;
}

const char* drv_first_active(enum drv_subsys s)
{
    int i;
    if ((int)s < 0 || (int)s >= DRV_SUBSYS_MAX) return 0;
    for (i = 0; i < DRV_PROVIDERS_MAX; i++) {
        if (g_slots[s][i].active && g_slots[s][i].ops) {
            return g_slots[s][i].ops->name;
        }
    }
    return 0;
}

static void put_str(const char* s)
{
    if (!s) return;
    while (*s) screen_put_char(*s++);
}

int drv_fallback_media(void)
{
    put_str("[drv: fallback media]\n");
    return 0;
}

int drv_fallback_link(void)
{
    put_str("[drv: fallback link (loopback)]\n");
    return 0;
}

int drv_fallback_screen(void)
{
    put_str("[drv: fallback screen (VGA text)]\n");
    return 0;
}

int drv_fallback_input(void)
{
    put_str("[drv: fallback input (no-op)]\n");
    return 0;
}

int drv_fallback_sound(void)
{
    put_str("[drv: fallback sound (silence)]\n");
    return 0;
}

int drv_fallback_rtc(void)
{
    put_str("[drv: fallback rtc (no-op)]\n");
    return 0;
}

int drv_fallback_guard(void)
{
    put_str("[drv: fallback guard (disabled)]\n");
    return 0;
}

int drv_fallback_bridge(void)
{
    put_str("[drv: fallback bridge (no-op)]\n");
    return 0;
}
