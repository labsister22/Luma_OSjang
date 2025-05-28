#include "header/driver/disk.h"
#include "header/cpu/portio.h"
#include "header/text/framebuffer.h"

static void ATA_busy_wait() {
    while (in(0x1F7) & ATA_STATUS_BSY); 
}

static void ATA_DRQ_wait() {
    while (!(in(0x1F7) & ATA_STATUS_RDY));
}

void read_blocks(void *ptr, uint32_t logical_block_address, uint8_t block_count) {
    ATA_busy_wait(); // Tunggu hingga perangkat siap
    out(0x1F6, 0xE0 | ((logical_block_address >> 24) & 0xF)); // Pilih drive dan LBA
    out(0x1F2, block_count); // Jumlah blok yang akan dibaca
    out(0x1F3, (uint8_t) logical_block_address); // LBA (byte 0-7)
    out(0x1F4, (uint8_t) (logical_block_address >> 8)); // LBA (byte 8-15)
    out(0x1F5, (uint8_t) (logical_block_address >> 16)); // LBA (byte 16-23)
    out(0x1F7, 0x20); // Perintah "Read Sectors"

    uint16_t *target = (uint16_t *)ptr;
    for (uint32_t i = 0; i < block_count; i++) {
        ATA_busy_wait(); // Tunggu hingga perangkat siap
        ATA_DRQ_wait(); // Tunggu hingga perangkat siap mengirim data
        for (uint32_t j = 0; j < HALF_BLOCK_SIZE; j++) {
            target[HALF_BLOCK_SIZE * i + j] = in16(0x1F0); // Baca data
        }
    }
}

void write_blocks(const void *ptr, uint32_t logical_block_address, uint8_t block_count) {
    ATA_busy_wait();
    out(0x1F6, 0xE0 | ((logical_block_address >> 24) & 0xF));
    out(0x1F2, block_count);
    out(0x1F3, (uint8_t) logical_block_address);
    out(0x1F4, (uint8_t) (logical_block_address >> 8));
    out(0x1F5, (uint8_t) (logical_block_address >> 16));
    out(0x1F7, 0x30); // WRITE SECTORS

    for (uint32_t i = 0; i < block_count; i++) {
        ATA_busy_wait();
        ATA_DRQ_wait();
        for (uint32_t j = 0; j < HALF_BLOCK_SIZE; j++) {
            out16(0x1F0, ((uint16_t*) ptr)[i * HALF_BLOCK_SIZE + j]);
        }
    }
}