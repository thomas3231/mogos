ENTRY(start)
SECTIONS
{
    . = 0x0;
    .text : {
        build/kernel_entry.o(.text)
        *(.text)
    }
    .data : { *(.data) }
    .rodata : { *(.rodata) }
    .bss : { *(.bss) }
}