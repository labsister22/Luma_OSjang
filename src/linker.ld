
ENTRY(loader)                /* the name of the entry label */

SECTIONS {
    . = 0x00100000;          /* the code should be loaded at 1 MB */

    .multiboot ALIGN (0x1000) :   /* align at 4 KB */
    {
        *(.multiboot)        /* GRUB multiboot header */
    } :text

    .text ALIGN (0x1000) :   /* align at 4 KB */
    {
        *(.text)             /* all text sections from all files */
    } :text

    .rodata ALIGN (0x1000) : /* align at 4 KB */
    {
        *(.rodata*)          /* all read-only data sections from all files */
    } :rodata

    .data ALIGN (0x1000) :   /* align at 4 KB */
    {
        *(.data)             /* all data sections from all files */
    } :data

    .bss ALIGN (0x1000) :    /* align at 4 KB */
    {
        *(COMMON)            /* all COMMON sections from all files */
        *(.bss)              /* all bss sections from all files */
    } :bss
}

/* Definisikan header segmen dengan hak akses yang benar */
PHDRS {
    text PT_LOAD FLAGS(5);   /* Read & Execute (R-X) */
    rodata PT_LOAD FLAGS(4); /* Read-only (R--) */
    data PT_LOAD FLAGS(6);   /* Read & Write (RW-) */
    bss PT_LOAD FLAGS(6);    /* Read & Write (RW-) */
}
