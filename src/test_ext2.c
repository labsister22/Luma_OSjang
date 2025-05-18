#include "header/filesystem/ext2.h"
#include "header/filesystem/test_ext2.h"
#include "header/driver/disk.h"
#include "header/text/framebuffer.h"
#include "header/driver/keyboard.h"
#include <stdint.h>
#include <stdbool.h>

void test_block_boundaries(void) {
    // First read the existing data to avoid corrupting the sample image
    struct BlockBuffer original;
    read_blocks(&original, 0, 1);
    
    // Make a backup before testing
    struct BlockBuffer backup = original;
    
    // Perform tests on a safe block (not 0 or 1 which might contain critical data)
    struct BlockBuffer buffer;
    buffer.buf[0] = 0xAA;
    write_blocks(&buffer, 17, 1);  // Use block 17 instead of 0
    
    // Verify
    struct BlockBuffer verify;
    read_blocks(&verify, 17, 1);
    if (verify.buf[0] != 0xAA) {
        framebuffer_write(0, 0, 'F', 0xF, 0x0);
    } else {
        framebuffer_write(0, 0, 'P', 0xF, 0x0);
    }
    
    // Restore original data
    write_blocks(&backup, 17, 1);
}

void test_directory_operations(void) {
    // Create test directory
    struct EXT2DriverRequest request = {
        .parent_inode = 1,  // Root directory
        .name = "test_dir",
        .name_len = 8,
        .buffer_size = 0,   // Directory
        .is_directory = true
    };
    
    int8_t status = write(request);
    if (status != 0) {
        framebuffer_write(0, 1, 'F', 0xF, 0x0);
        return;
    }
    
    // Try reading it back
    status = read_directory(&request);
    if (status == 0) {
        framebuffer_write(0, 1, 'P', 0xF, 0x0);
    } else {
        framebuffer_write(0, 1, 'F', 0xF, 0x0);
    }
}

// Add this to your kernel.c to run test

void run_ext2_tests(void) {
    // Run all filesystem tests
    test_block_boundaries();
    test_directory_operations();
    
    // Print test completion message
    framebuffer_write(0, 2, 'T', 0xF, 0x0);
    framebuffer_write(0, 3, 'e', 0xF, 0x0);
    framebuffer_write(0, 4, 's', 0xF, 0x0);
    framebuffer_write(0, 5, 't', 0xF, 0x0);
    framebuffer_write(0, 6, 's', 0xF, 0x0);
    framebuffer_write(0, 7, ' ', 0xF, 0x0);
    framebuffer_write(0, 8, 'D', 0xF, 0x0);
    framebuffer_write(0, 9, 'o', 0xF, 0x0);
    framebuffer_write(0, 10, 'n', 0xF, 0x0);
    framebuffer_write(0, 11, 'e', 0xF, 0x0);
}