# Compiler & Linker
ASM           = nasm
LIN           = ld
CC            = gcc
ISO           = genisoimage

# Directory
SOURCE_FOLDER = src
OUTPUT_FOLDER = bin
ISO_NAME      = OS2025
DISK_NAME     = storage

# Flags
WARNING_CFLAG = -Wall -Wextra -Werror
DEBUG_CFLAG   = -fshort-wchar -g
STRIP_CFLAG   = -nostdlib -fno-stack-protector -nostartfiles -nodefaultlibs -ffreestanding
CFLAGS        = $(DEBUG_CFLAG) $(WARNING_CFLAG) $(STRIP_CFLAG) -m32 -c -I$(SOURCE_FOLDER) -Iheader
AFLAGS        = -f elf32 -g -F dwarf
LFLAGS        = -T $(SOURCE_FOLDER)/linker.ld -melf_i386

# File Object
OBJS = $(OUTPUT_FOLDER)/kernel-entrypoint.o \
       $(OUTPUT_FOLDER)/kernel.o \
       $(OUTPUT_FOLDER)/gdt.o \
       $(OUTPUT_FOLDER)/portio.o \
       $(OUTPUT_FOLDER)/framebuffer.o \
       $(OUTPUT_FOLDER)/interrupt.o \
       $(OUTPUT_FOLDER)/idt.o	\
       $(OUTPUT_FOLDER)/keyboard.o	\
       $(OUTPUT_FOLDER)/intsetups.o	\
       $(OUTPUT_FOLDER)/string.o \
	   $(OUTPUT_FOLDER)/disk.o


# Run QEMU
run: all
	@qemu-system-i386 -s -S -drive file=bin/storage.bin,format=raw,if=ide,index=0,media=disk -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso


# Build All
all: build

build: iso

# Clean
clean:

	rm -rf $(OBJS) $(OUTPUT_FOLDER)/kernel $(OUTPUT_FOLDER)/$(ISO_NAME).iso \
		$(OUTPUT_FOLDER)/iso $(OUTPUT_FOLDER)/$(DISK_NAME).bin \

#Disk
disk:
	@qemu-img create -f raw $(OUTPUT_FOLDER)/$(DISK_NAME).bin 4M

# Compile Kernel Entry Point (Assembly)
$(OUTPUT_FOLDER)/kernel-entrypoint.o: $(SOURCE_FOLDER)/kernel-entrypoint.s
	@$(ASM) $(AFLAGS) $< -o $@


# Compile intsetup (Assembly)
$(OUTPUT_FOLDER)/intsetups.o: $(SOURCE_FOLDER)/intsetups.s
	$(ASM) $(AFLAGS) $< -o $@

# Compile Kernel (C)
$(OUTPUT_FOLDER)/kernel.o: $(SOURCE_FOLDER)/kernel.c
	$(CC) $(CFLAGS) $< -o $@

# Compile GDT (C)
$(OUTPUT_FOLDER)/gdt.o: $(SOURCE_FOLDER)/gdt.c
	$(CC) $(CFLAGS) $< -o $@

# Compile portio (C)
$(OUTPUT_FOLDER)/portio.o: $(SOURCE_FOLDER)/portio.c
	$(CC) $(CFLAGS) $< -o $@

# Compile framebuffer (C)
$(OUTPUT_FOLDER)/framebuffer.o: $(SOURCE_FOLDER)/framebuffer.c
	$(CC) $(CFLAGS) $< -o $@

# Compile interrupt (C)
$(OUTPUT_FOLDER)/interrupt.o: $(SOURCE_FOLDER)/interrupt.c
	$(CC) $(CFLAGS) $< -o $@

# Compile idt (C)
$(OUTPUT_FOLDER)/idt.o: $(SOURCE_FOLDER)/idt.c
	$(CC) $(CFLAGS) $< -o $@

# Compile keyboard (C)
$(OUTPUT_FOLDER)/keyboard.o: $(SOURCE_FOLDER)/keyboard.c
	$(CC) $(CFLAGS) $< -o $@

# Compile string (C)
$(OUTPUT_FOLDER)/string.o: $(SOURCE_FOLDER)/string.c
	$(CC) $(CFLAGS) $< -o $@

# Compile disk (C)
$(OUTPUT_FOLDER)/disk.o: $(SOURCE_FOLDER)/disk.c
	$(CC) $(CFLAGS) $< -o $@

# Link Semua Object Files
$(OUTPUT_FOLDER)/kernel: $(OBJS)
	@$(LIN) $(LFLAGS) $(OBJS) -o $@
	@echo "Linking object files and generating ELF32 kernel..."

# Generate ISO
iso: $(OUTPUT_FOLDER)/kernel
	@mkdir -p $(OUTPUT_FOLDER)/iso/boot/grub
	@cp $(OUTPUT_FOLDER)/kernel       $(OUTPUT_FOLDER)/iso/boot/
	@cp other/grub1                   $(OUTPUT_FOLDER)/iso/boot/grub/
	@cp $(SOURCE_FOLDER)/menu.lst     $(OUTPUT_FOLDER)/iso/boot/grub/
	@$(ISO) -R                          \
		-b boot/grub/grub1              \
		-no-emul-boot                   \
		-boot-load-size 4               \
		-A os                           \
		-input-charset utf8             \
		-quiet                          \
		-boot-info-table                \
		-o $(OUTPUT_FOLDER)/$(ISO_NAME).iso \
		$(OUTPUT_FOLDER)/iso

