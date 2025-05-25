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
    ; Disable interrupts during setup
    cli
    
    ; Set up stack
    mov esp, stack_top
    mov ebp, esp
    
    ; Clear direction flag
    cld
    
    ; Call kernel setup
    call kernel_setup
    
    ; If kernel_setup returns, something went wrong
    jmp hang

hang:
    cli     ; Ensure interrupts are disabled
    hlt     ; Halt processor
    jmp hang ; Loop in case of NMI or other wake-up

; Load GDT function
load_gdt:
    push ebp
    mov ebp, esp
    push ebx    ; Save registers
    push ecx
    push edx
    
    mov eax, [ebp + 8]   ; Get GDTR pointer from parameter
    
    ; Validate pointer is not null (basic check)
    test eax, eax
    jz .error
    
    lgdt [eax]           ; Load GDT
    
    ; Reload segment registers with kernel data segment
    mov ax, 0x10         ; Kernel data segment selector (assuming GDT entry 2)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Far jump to reload CS with kernel code segment
    jmp 0x08:.reload_cs  ; Assuming GDT entry 1 is kernel code
.reload_cs:
    ; Restore registers
    pop edx
    pop ecx
    pop ebx
    pop ebp
    ret

.error:
    ; GDT pointer was null - halt system
    jmp hang

; Set TSS register
set_tss_register:
    push eax
    push ebx
    
    mov ax, 0x28         ; TSS segment selector
    
    ; Validate TSS selector before loading
    mov bx, ax
    test bx, bx
    jz .tss_error
    
    ltr ax               ; Load Task Register
    
    pop ebx
    pop eax
    ret

.tss_error:
    pop ebx
    pop eax
    jmp hang

; Execute user program
kernel_execute_user_program:
    push ebp
    mov ebp, esp
    push eax
    push ebx
    push ecx
    push edx
    
    mov eax, [ebp + 8]   ; Get user program address
    
    ; Validate user program address
    test eax, eax
    jz .user_error
    
    ; Check if address is in valid user space (example: >= 0x400000)
    cmp eax, 0x400000
    jb .user_error
    
    ; Setup user mode segments
    mov ax, 0x23         ; User data segment (0x20 | 3 for ring 3)
    mov ds, ax
    mov es, ax 
    mov fs, ax
    mov gs, ax
    
    ; Get original user program address for iret
    mov eax, [ebp + 8]
    
    ; Setup stack for IRET to user mode
    ; Stack layout for IRET: SS, ESP, EFLAGS, CS, EIP
    push 0x23            ; User data segment (SS)
    push 0x800000        ; User stack pointer (higher address, more safe)
    
    ; Setup EFLAGS
    pushf                ; Get current EFLAGS
    pop ebx
    or ebx, 0x200        ; Enable interrupts (IF flag)
    and ebx, 0xFFFFBFFF  ; Clear NT flag (bit 14)
    push ebx             ; Push modified EFLAGS
    
    push 0x1B            ; User code segment (0x18 | 3 for ring 3)
    push eax             ; User program address (EIP)
    
    ; Clear registers for security
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    
    ; Jump to user mode
    iret
    
.user_error:
    ; Invalid user program - restore stack and return error
    pop edx
    pop ecx
    pop ebx
    pop eax
    pop ebp
    ; Return error code in EAX
    mov eax, -1
    ret

section .bss
align 16                ; Better alignment for stack
stack_bottom:
    resb 65536          ; 64KB stack - much more reasonable
stack_top: