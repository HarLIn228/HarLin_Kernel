#include "harlin_API.h"

void __attribute__((section(".text.kernel_main"))) kernel_main(void)
{
    Harlin_Boot();
}