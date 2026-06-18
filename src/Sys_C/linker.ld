ENTRY(kernel_main)

SECTIONS
{
    . = 0x10000;
    
    .text :
    {
        *(.text.kernel_main)
        *(.text)
    }
    
    .rodata :
    {
        *(.rodata)
    }
    
    .data :
    {
        *(.data)
    }
    
    .bss :
    {
        *(COMMON)
        *(.bss)
    }
}
