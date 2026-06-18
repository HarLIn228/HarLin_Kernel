#include "keyboard.h"
#include "shell.h"
#include "network.h"
#include "screen.h"

void kernel_main(void)
{
    keyboard_init();
    network_init();
    screen_clear();
    shell_run();
}