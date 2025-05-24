global loader
global load_gdt
global set_tss_register
global kernel_execute_user_program
extern kernel_setup

MAGIC_NUMBER equ 0x1BADB002
FLAGS        equ 0x0
CHECKSUM     equ -MAGIC_NUMBER

section .multiboot
align 4
    dd MAGIC_NUMBER
    dd FLAGS
    dd CHECKSUM

section .setup.text
loader:
    mov esp, stack_top
    call kernel_setup
    
hang:
    hlt
    jmp hang

; Load GDT function
load_gdt:
    push ebp
    mov ebp, esp
    
    mov eax, [ebp + 8]   ; Get GDTR pointer from parameter
    lgdt [eax]           ; Load GDT
    
    ; Reload segment registers
    mov ax, 0x10         ; Kernel data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Far jump to reload CS with kernel code segment
    jmp 0x08:.reload_cs
.reload_cs:
    pop ebp
    ret

; Set TSS register
set_tss_register:
    mov ax, 0x28    ; TSS segment selector (GDT_TSS_SELECTOR)
    ltr ax          ; Load Task Register
    ret

; Execute user program
kernel_execute_user_program:
    push ebp
    mov ebp, esp
    
    mov eax, [ebp + 8]  ; Get user program address
    
    ; Setup user mode segments
    mov ax, 0x23        ; User data segment (0x20 | 3 for ring 3)
    mov ds, ax
    mov es, ax 
    mov fs, ax
    mov gs, ax
    
    ; Setup stack for user mode
    push 0x23           ; User data segment 
    push 0x400000       ; User stack pointer
    pushf               ; EFLAGS
    pop eax
    or eax, 0x200       ; Enable interrupts
    push eax
    push 0x1B           ; User code segment (0x18 | 3 for ring 3)
    push dword [ebp + 8] ; User program address
    
    iret                ; Jump to user mode
    
    ; Should never reach here
    pop ebp
    ret

section .bss
align 4
stack_bottom:
    resb 16384
stack_top: