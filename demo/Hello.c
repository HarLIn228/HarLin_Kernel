void _start(void)
{
    const char* msg = "Hello HarLin Kernel\n";

    __asm__ volatile (
        "mov $1, %%rax\n"
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :
        : "r"(msg)
        : "rax", "rdi", "memory"
    );

    __asm__ volatile (
        "xor %%rax, %%rax\n"
        "int $0x80\n"
        :
        :
        : "rax"
    );

    for (;;)
        __asm__ volatile ("hlt");
}
