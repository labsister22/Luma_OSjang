global loader       ; the entry symbol for ELF
global load_gdt     ; load GDT table
extern kernel_setup ; kernel

KERNEL_STACK_SIZE equ 2097152           ; size of stack in bytes
MAGIC_NUMBER      equ 0x1BADB002     ; define the magic number constant
FLAGS             equ 0x0            ; multiboot flags
CHECKSUM          equ -MAGIC_NUMBER  ; calculate the checksum
                                     ; (magic number + checksum + flags should equal 0)

section .bss
align 4                              ; align at 4 bytes
kernel_stack:                        ; label points to beginning of memory
    resb KERNEL_STACK_SIZE           ; reserve stack for the kernel

section .multiboot                   ; GNU GRUB Multiboot header
align 4                              ; the code must be 4 byte aligned
    dd MAGIC_NUMBER                  ; write the magic number to the machine code,
    dd FLAGS                         ; the flags,
    dd CHECKSUM                      ; and the checksum


section .text                                  ; start of the text (code) 
loader:                                        ; the loader label (defined as entry point in linker script)
    mov  esp, kernel_stack + KERNEL_STACK_SIZE ; setup stack register to proper location
    call kernel_setup
.loop:
    jmp .loop                                  ; loop forever
global kernel_execute_user_program ; execute initial user program from kernel
kernel_execute_user_program:
    mov  eax, 0x20 | 0x3
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    
    ; Using iret (return instruction for interrupt) technique for privilege change
    ; Stack values will be loaded into these register:
    ; [esp] -> eip, [esp+4] -> cs, [esp+8] -> eflags, [] -> user esp, [] -> user ss
    mov  ecx, [esp+4] ; Save first (before pushing anything to stack) for last push
    push eax ; Stack segment selector (GDT_USER_DATA_SELECTOR), user privilege
    mov  eax, ecx
    add  eax, 0x400000 - 4
    push eax ; User space stack pointer (esp), move it into last 4 MiB
    pushf    ; eflags register state, when jump inside user program
    mov  eax, 0x18 | 0x3
    push eax ; Code segment selector (GDT_USER_CODE_SELECTOR), user privilege
    mov  eax, ecx
    push eax ; eip register to jump back

    iret

global set_tss_register
set_tss_register:
    ; Load TSS selector (0x28) into TR register
    mov ax, 0x28       ; GDT_TSS_SELECTOR (typically 0x28 for TSS with ring 0)
    ltr ax             ; Load task register with TSS selector
    ret

; More details: https://en.wikibooks.org/wiki/X86_Assembly/Protected_Mode
load_gdt:
    cli
    mov  eax, [esp+4]
    lgdt [eax] ; Load GDT from GDTDescriptor, eax at this line will point GDTR location

    ; Set bit-0 (Protection Enable bit-flag) in Control Register 0 (CR0)
    ; This is optional, as usually GRUB already start with protected mode flag enabled
    mov  eax, cr0
    or   eax, 1
    mov  cr0, eax

    ; Far jump to update cs register
    ; Warning: Invalid GDT will raise exception in following instruction below
    jmp 0x8:flush_cs
flush_cs:
    mov ax, 10h ; Update all segment register
    mov ss, ax
    mov ds, ax
    mov es, ax
    ret