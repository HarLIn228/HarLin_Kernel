#ifndef ACPI_H
#define ACPI_H

#include "harlin_API.h"

int  acpi_init(void);
void acpi_power_off(void);
void acpi_reboot(void);

#define Harlin_AcpiInit               acpi_init
#define Harlin_AcpiPowerOff           acpi_power_off
#define Harlin_AcpiReboot             acpi_reboot

#endif
