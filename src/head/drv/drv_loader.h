#ifndef DRV_LOADER_H
#define DRV_LOADER_H

#include "harlin_API.h"

#define DRV_SUBSYS_MAX    32
#define DRV_NAME_MAX      32
#define DRV_PROVIDERS_MAX 16

enum drv_subsys {
    DRV_SUBSYS_MEDIA = 0,
    DRV_SUBSYS_LINK,
    DRV_SUBSYS_USB,
    DRV_SUBSYS_SCREEN,
    DRV_SUBSYS_SOUND,
    DRV_SUBSYS_INPUT,
    DRV_SUBSYS_CLOCK,
    DRV_SUBSYS_POWER,
    DRV_SUBSYS_BRIDGE,
    DRV_SUBSYS_SERIO,
    DRV_SUBSYS_TTY,
    DRV_SUBSYS_RTC,
    DRV_SUBSYS_GUARD,
    DRV_SUBSYS_COOL,
    DRV_SUBSYS_PIN,
    DRV_SUBSYS_CIPHER,
    DRV_SUBSYS_BLOCK,
    DRV_SUBSYS_HMI,
    DRV_SUBSYS_FS,
    DRV_SUBSYS_AGGREGATE,
    DRV_SUBSYS_PULSE,
    DRV_SUBSYS_WAVE,
    DRV_SUBSYS_SENSE,
    DRV_SUBSYS_BOARD,
    DRV_SUBSYS_SCSI,
    DRV_SUBSYS_FLASH,
    DRV_SUBSYS_VIRT,
    DRV_SUBSYS_GPU,
    DRV_SUBSYS_RESERVED28,
    DRV_SUBSYS_RESERVED29,
    DRV_SUBSYS_RESERVED30,
    DRV_SUBSYS_RESERVED31
};

struct drv_ops {
    const char* name;
    enum drv_subsys subsys;
    int  (*probe)(void* ctx);
    int  (*init)(void* ctx);
    void (*remove)(void* ctx);
    int  (*suspend)(void* ctx);
    int  (*resume)(void* ctx);
    int  priority;
};

struct drv_slot {
    const struct drv_ops* ops;
    void* ctx;
    int active;
};

struct drv_provider {
    const char* name;
    int  (*scan)(void);
    int  available;
};

void drv_loader_init(void);
int  drv_register(const struct drv_ops* ops, void* ctx);
int  drv_unregister(const char* name);
int  drv_load_subsys(enum drv_subsys s);
int  drv_load_all(void);
int  drv_active_count(enum drv_subsys s);
const char* drv_first_active(enum drv_subsys s);
const char* drv_subsys_name(enum drv_subsys s);

int  drv_provider_register(const struct drv_provider* p);
int  drv_provider_scan_all(void);
int  drv_provider_available(const char* name);

#define Harlin_DrvLoaderInit          drv_loader_init
#define Harlin_DrvRegister            drv_register
#define Harlin_DrvUnregister          drv_unregister
#define Harlin_DrvLoadSubsys          drv_load_subsys
#define Harlin_DrvLoadAll             drv_load_all
#define Harlin_DrvActiveCount         drv_active_count
#define Harlin_DrvFirstActive         drv_first_active
#define Harlin_DrvSubsysName          drv_subsys_name
#define Harlin_DrvProviderRegister    drv_provider_register
#define Harlin_DrvProviderScanAll     drv_provider_scan_all
#define Harlin_DrvProviderAvailable   drv_provider_available

int  drv_fallback_media(void);
int  drv_fallback_link(void);
int  drv_fallback_screen(void);
int  drv_fallback_input(void);
int  drv_fallback_sound(void);
int  drv_fallback_rtc(void);
int  drv_fallback_guard(void);
int  drv_fallback_bridge(void);

#endif
