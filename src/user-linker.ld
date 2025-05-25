ENTRY(_start)

SECTIONS {
    . = 0x400000;  /* Load address must match kernel's execution address */
    
    .text ALIGN(4): {
        [a-z\/]*crt0.o(.text)  /* Put the entrypoint in the front of executable */
        *(.text)
    }

    .data ALIGN(4): {
        *(.data)
    }

    .bss ALIGN(4): {
        *(COMMON)
        *(.bss)
    }

    .rodata ALIGN(4): {
        *(.rodata*)
    }
    _linker_user_program_end = .;
    ASSERT ((_linker_user_program_end <= 0x400000 + 1 * 1024 * 1024), "Error: User program linking result is more than 1 MiB")
}