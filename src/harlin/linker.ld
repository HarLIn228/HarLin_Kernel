ENTRY(kernel_main)

PHDRS
{
    text PT_LOAD FLAGS((1 << 0) | (1 << 2));
    rodata PT_LOAD FLAGS((1 << 2));
    data PT_LOAD FLAGS((1 << 0) | (1 << 2));
}

SECTIONS
{
    . = 0x10000;

    .text : {
        *(.text.kernel_main)
        *(.text)
    } :text

    . = ALIGN(0x1000);

    .rodata : {
        *(.rodata)
    } :rodata

    . = ALIGN(0x1000);

    .data : {
        *(.data)
    } :data

    . = ALIGN(0x1000);

    .bss : {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        __bss_end = .;
    } :data

    /DISCARD/ : {
        *(.note)
        *(.note.*)
        *(.comment)
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.gnu*)
        *(.rel.*)
        *(.rela.*)
    }
}
