#include "harlin.h"

void _start(void)
{
    if (harlin_exec("shell.chc") != 0)
        harlin_exit(1);
    for (;;) {
        harlin_yield();
    }
}
