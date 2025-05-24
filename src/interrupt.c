#include "header/cpu/interrupt.h"
#include "header/cpu/portio.h"
#include "header/driver/keyboard.h"
#include "header/cpu/gdt.h"
#include "header/filesystem/test_ext2.h"
#include "header/filesystem/ext2.h"
#include "header/text/framebuffer.h"

struct TSSEntry _interrupt_tss_entry = {
    .ss0  = GDT_KERNEL_DATA_SEGMENT_SELECTOR,
};

void set_tss_kernel_current_stack(void) {
    uint32_t stack_ptr;
    // Reading base stack frame instead esp
    __asm__ volatile ("mov %%ebp, %0": "=r"(stack_ptr) : /* <Empty> */);
    // Add 8 because 4 for ret address and other 4 is for stack_ptr variable
    _interrupt_tss_entry.esp0 = stack_ptr + 8; 
}

void io_wait(void)
{
    out(0x80, 0);
}

void pic_ack(uint8_t irq)
{
    if (irq >= 8){
        out(PIC2_COMMAND, PIC_ACK);
    }
    out(PIC1_COMMAND, PIC_ACK);
}

void pic_remap(void)
{
    // Starts the initialization sequence in cascade mode
    out(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out(PIC1_DATA, PIC1_OFFSET); // ICW2: Master PIC vector offset
    io_wait();
    out(PIC2_DATA, PIC2_OFFSET); // ICW2: Slave PIC vector offset
    io_wait();
    out(PIC1_DATA, 0b0100); // ICW3: tell Master PIC, slave PIC at IRQ2 (0000 0100)
    io_wait();
    out(PIC2_DATA, 0b0010); // ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();

    out(PIC1_DATA, ICW4_8086);
    io_wait();
    out(PIC2_DATA, ICW4_8086);
    io_wait();

    // Disable all interrupts
    out(PIC1_DATA, PIC_DISABLE_ALL_MASK);
    out(PIC2_DATA, PIC_DISABLE_ALL_MASK);
}

void main_interrupt_handler(struct InterruptFrame frame)
{
    uint32_t int_num = frame.int_number;

    switch (int_num)
    {
    case 0x00:
        // Division by zero - should handle properly
        break;

    case 0x0D:
        // General Protection Fault - critical error
        // Add error handling here instead of just breaking
        break;

    case 0x0E:
        // Page Fault - critical error
        // Add error handling here instead of just breaking
        break;

    case 0x21:
        // Keyboard interrupt (IRQ1)
        keyboard_isr();
        // PIC ACK will be handled below
        break;

    case 0x30:
        // System call interrupt
        syscall(frame);
        // No PIC ACK needed for software interrupts
        return;

    default:
        // Unknown interrupt - might want to log or handle
        break;
    }

    // Send End of Interrupt (EOI) for hardware interrupts only
    if (int_num >= PIC1_OFFSET && int_num <= PIC2_OFFSET + 7)
    {
        pic_ack(int_num - PIC1_OFFSET);
    }
}

void activate_keyboard_interrupt(void)
{
    out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_KEYBOARD));
}

void syscall(struct InterruptFrame frame) {
    // Add bounds checking and null pointer checks
    switch (frame.cpu.general.eax) {
        case 0:
            // Read syscall
            if (frame.cpu.general.ebx != 0 && frame.cpu.general.ecx != 0) {
                *((int8_t*) frame.cpu.general.ecx) = read(
                    *(struct EXT2DriverRequest*) frame.cpu.general.ebx
                );
            }
            break;
            
        case 1:
            // Read directory syscall
            if (frame.cpu.general.ebx != 0 && frame.cpu.general.ecx != 0) {
                *((int8_t*) frame.cpu.general.ecx) = read_directory(
                    (struct EXT2DriverRequest*) frame.cpu.general.ebx
                );
            }
            break;
            
        case 2:
            // Write syscall
            if (frame.cpu.general.ebx != 0 && frame.cpu.general.ecx != 0) {
                *((int8_t*) frame.cpu.general.ecx) = write(
                    *(struct EXT2DriverRequest*) frame.cpu.general.ebx
                );
            }
            break;
            
        case 3:
            // Delete syscall
            if (frame.cpu.general.ebx != 0 && frame.cpu.general.ecx != 0) {
                *((int8_t*) frame.cpu.general.ecx) = delete(
                    *(struct EXT2DriverRequest*) frame.cpu.general.ebx
                );
            }
            break;
            
        case 4: 
            // Get keyboard buffer
            if (frame.cpu.general.ebx != 0) {
                get_keyboard_buffer((char*) frame.cpu.general.ebx);
            }
            break;
        case 6:
            // Put string with bounds checking
            if (frame.cpu.general.ebx != 0 && 
                ((uint32_t)frame.cpu.general.ebx) >= 0x1000 && 
                ((uint32_t)frame.cpu.general.ebx) < 0x1000000) {
                puts(
                    (char*) frame.cpu.general.ebx, 
                    (uint8_t) frame.cpu.general.ecx, 
                    (uint8_t) frame.cpu.general.edx,
                    0
                );
            }
            break;
            
        case 7: 
            // Activate keyboard state
            keyboard_state_activate();
            break;
            
        case 8:
            // Clear framebuffer
            framebuffer_clear();
            framebuffer_set_cursor(0, 0);
            break;
            
        default:
            // Unknown syscall - do nothing
            break;
    }
}