global loader                        ; the entry symbol for ELF
global load_gdt                      ; load GDT table
global set_tss_register              ; set tss register to GDT entry
extern kernel_setup                  ; kernel C entrypoint
extern _paging_kernel_page_directory ; kernel page directory

KERNEL_VIRTUAL_BASE equ 0xC0000000    ; kernel virtual memory
KERNEL_STACK_SIZE   equ 2097152       ; size of stack in bytes
MAGIC_NUMBER        equ 0x1BADB002    ; define the magic number constant
FLAGS               equ 0x0           ; multiboot flags
CHECKSUM            equ -MAGIC_NUMBER ; calculate the checksum (magic number + checksum + flags == 0)


section .bss
align 4096                        ; align at 4KB (page boundary)
kernel_stack_phys:                ; label untuk stack fisik (sebelum paging)
    resb KERNEL_STACK_SIZE        ; alokasi stack untuk boot awal
kernel_stack:                     ; label untuk stack virtual (setelah paging)
    resb KERNEL_STACK_SIZE        ; alokasi stack untuk kernel setelah paging


section .multiboot  ; GRUB multiboot header
align 4             ; the code must be 4 byte aligned
    dd MAGIC_NUMBER ; write the magic number to the machine code,
    dd FLAGS        ; the flags,
    dd CHECKSUM     ; and the checksum


; start of the text (code) section
section .setup.text 
loader:                       ; the loader label (entry point in linker script)
    ; IMPORTANT: No code before this point can reference virtual addresses
    
    ; Initialize stack pointer - use physical address for now
    mov esp, kernel_stack_phys + KERNEL_STACK_SIZE

    ; Debug marker 'A' - boot started
    mov byte [0xB8000], 'A'
    mov byte [0xB8001], 0x0F

    ; Setup paging: load page directory
    mov eax, (_paging_kernel_page_directory - KERNEL_VIRTUAL_BASE)
    mov cr3, eax
    
    ; Debug marker 'B' - page directory loaded
    mov byte [0xB8002], 'B'
    mov byte [0xB8003], 0x0F
    
    ; Enable PSE (4MB pages)
    mov eax, cr4
    or eax, 0x00000010
    mov cr4, eax
    
    ; Debug marker 'C' - PSE enabled
    mov byte [0xB8004], 'C'
    mov byte [0xB8005], 0x0F
    
    ; Add significant delay to ensure everything is stable
    mov ecx, 1000000
delay_loop:
    loop delay_loop
    
    ; Debug marker 'D' - about to enable paging
    mov byte [0xB8006], 'D'
    mov byte [0xB8007], 0x0F
    
    ; Enable paging
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    
    ; Debug marker 'E' - paging enabled
    mov byte [0xB8008], 'E'
    mov byte [0xB8009], 0x0F

    ; Jump to higher half kernel
    ; We must use an absolute jump here, not a relative one
    mov eax, higher_half
    jmp eax

; Higher half code - this will be at virtual address 0xC0100000+
section .text
higher_half:
    ; Debug marker 'F' - in higher half
    mov byte [0xC00B800A], 'F'
    mov byte [0xC00B800B], 0x0F

    ; Now we're in the higher half kernel, set up the stack properly
    mov esp, kernel_stack + KERNEL_STACK_SIZE
    
    ; Remove identity mapping now that we're in higher half
    ; We can access the page directory through its virtual address now
    mov eax, _paging_kernel_page_directory
    mov dword [eax], 0  ; Clear entry 0
    invlpg [0]          ; Flush TLB for this mapping
    
    ; Debug marker 'G' - stack set up
    mov byte [0xC00B800C], 'G'
    mov byte [0xC00B800D], 0x0F
    
    ; Debug marker 'H' - about to call kernel_setup
    mov byte [0xC00B800E], 'H'
    mov byte [0xC00B800F], 0x0F
    
    ; Call C kernel setup function
    call kernel_setup
    
    ; Debug marker 'X' - should never get here
    mov byte [0xC00B8010], 'X'
    mov byte [0xC00B8011], 0x0F

.loop:
    hlt                         ; halt processor until next interrupt  
    jmp .loop                   ; loop forever


section .text
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
    ; Warning: Invalid GDT will raise exception in any instruction below
    jmp 0x8:flush_cs
flush_cs:
    ; Update all segment register
    mov ax, 10h
    mov ss, ax
    mov ds, ax
    mov es, ax
    ret


set_tss_register:
    mov ax, 0x28 | 0 ; GDT TSS Selector, ring 0
    ltr ax
    ret