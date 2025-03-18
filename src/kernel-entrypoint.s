global loader
global load_gdt
global loop        ; Tambahkan agar bisa digunakan di mana saja
extern kernel_setup

KERNEL_STACK_SIZE equ 4096
MAGIC_NUMBER      equ 0x1BADB002
FLAGS             equ 0x0
CHECKSUM          equ -MAGIC_NUMBER

section .bss
align 4
kernel_stack:
    resb KERNEL_STACK_SIZE

section .multiboot
align 4
    dd MAGIC_NUMBER
    dd FLAGS
    dd CHECKSUM

section .text
loader:
    mov  esp, kernel_stack + KERNEL_STACK_SIZE
    call kernel_setup
loop:
    jmp loop   ; Ubah .loop menjadi loop

load_gdt:
    cli
    mov  eax, [esp+4]
    test eax, eax
    jz   loop   ; Gunakan loop, bukan .loop
    lgdt [eax]

    mov  eax, cr0
    or   eax, 1
    mov  cr0, eax

    jmp 0x8:flush_cs
flush_cs:
    mov ax, 10h
    mov ss, ax
    mov ds, ax
    mov es, ax
    jmp loop   ; Gunakan loop, bukan .loop

section .note.GNU-stack noalloc
