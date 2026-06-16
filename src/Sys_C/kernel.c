#include "keyboard.h"
#include "shell.h"

void kernel_main(void)
{
    keyboard_init();
    shell_run();
}