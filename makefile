# Compiler & Linker
ASM           = nasm
LIN           = ld
CC            = gcc
ISO           = genisoimage

# Directory
SOURCE_FOLDER = src
OUTPUT_FOLDER = bin
ISO_NAME      = OS2025

# Flags
WARNING_CFLAG = -Wall -Wextra -Werror
DEBUG_CFLAG   = -fshort-wchar -g
STRIP_CFLAG   = -nostdlib -fno-stack-protector -nostartfiles -nodefaultlibs -ffreestanding
CFLAGS = $(DEBUG_CFLAG) $(WARNING_CFLAG) $(STRIP_CFLAG) -m32 -c -I$(SOURCE_FOLDER) -Iheader -g -gdwarf-2
AFLAGS = -f elf32 -g -F dwarf
LFLAGS        = -T $(SOURCE_FOLDER)/linker.ld -melf_i386
DISK_NAME     = storage

# File Object
OBJS = $(OUTPUT_FOLDER)/kernel-entrypoint.o \
       $(OUTPUT_FOLDER)/kernel.o \
       $(OUTPUT_FOLDER)/gdt.o \
       $(OUTPUT_FOLDER)/portio.o \
       $(OUTPUT_FOLDER)/framebuffer.o \
       $(OUTPUT_FOLDER)/interrupt.o \
	   $(OUTPUT_FOLDER)/intsetup.o \
       $(OUTPUT_FOLDER)/idt.o	\
       $(OUTPUT_FOLDER)/keyboard.o	\
       $(OUTPUT_FOLDER)/disk.o	\
       $(OUTPUT_FOLDER)/string.o \
       $(OUTPUT_FOLDER)/ext2.o \
       $(OUTPUT_FOLDER)/test_ext2.o\
	   $(OUTPUT_FOLDER)/cmos.o \
       $(OUTPUT_FOLDER)/paging.o \
			$(OUTPUT_FOLDER)/process.o \
			$(OUTPUT_FOLDER)/scheduler.o \
			$(OUTPUT_FOLDER)/context-switch.o			  

# Run QEMU
run: all
	@qemu-system-i386 -s -rtc base=localtime -drive file=bin/storage.bin,format=raw,if=ide,index=0,media=disk -cdrom bin/OS2025.iso

# run: iso
# 	qemu-system-i386 -s -S -cdrom $(OUTPUT_FOLDER)/OS2025.iso

# Build All - updated to include everything needed
all: iso disk insert-shell inserter user-shell

# Build just the kernel
build: iso disk insert-shell inserter user-shell

# Clean
clean:
	rm -rf $(OBJS) $(OUTPUT_FOLDER)/kernel $(OUTPUT_FOLDER)/$(ISO_NAME).iso \
        $(OUTPUT_FOLDER)/iso $(OUTPUT_FOLDER)/storage.bin  $(OUTPUT_FOLDER)/shell \
        $(OUTPUT_FOLDER)/shell_elf $(OUTPUT_FOLDER)/inserter *.o

# Disk
.PHONY: disk
disk:
	@mkdir -p bin
	@rm -f bin/storage.bin
	@qemu-img create -f raw bin/storage.bin 4M
	@echo "Storage file bin/storage.bin dibuat."

# Inserter
inserter:
	@mkdir -p $(OUTPUT_FOLDER)
	@$(CC) -Wno-builtin-declaration-mismatch -g -I$(SOURCE_FOLDER) \
        -fstack-protector-strong -D_FORTIFY_SOURCE=2 \
        $(SOURCE_FOLDER)/string.c \
        $(SOURCE_FOLDER)/ext2.c \
        $(SOURCE_FOLDER)/external-inserter.c \
        -o $(OUTPUT_FOLDER)/inserter \
        -DDEBUG_MODE

user-shell:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-shell.c -o user-shell.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/builtin_commands.c -o builtin_commands.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/stdlib/string.c -o string.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		crt0.o user-shell.o builtin_commands.o string.o -o $(OUTPUT_FOLDER)/shell
	@echo Linking object shell object files and generate flat binary...
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=elf32-i386 \
		crt0.o user-shell.o builtin_commands.o string.o -o $(OUTPUT_FOLDER)/shell_elf
	@echo Linking object shell object files and generate ELF32 for debugging...
	@size --target=binary $(OUTPUT_FOLDER)/shell
	@rm -f *.o

insert-shell: disk inserter user-shell
	@echo Inserting shell into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter shell 2 $(DISK_NAME).bin

# Compile Kernel Entry Point (Assembly)
$(OUTPUT_FOLDER)/kernel-entrypoint.o: $(SOURCE_FOLDER)/kernel-entrypoint.s
	@$(ASM) $(AFLAGS) $< -o $@

# Compile intsetup (Assembly)
$(OUTPUT_FOLDER)/intsetup.o: $(SOURCE_FOLDER)/intsetup.s
	$(ASM) $(AFLAGS) $< -o $@

# Compile context-switch (Assembly)
$(OUTPUT_FOLDER)/context-switch.o: $(SOURCE_FOLDER)/context-switch.s
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

# Compile disk (C)
$(OUTPUT_FOLDER)/disk.o: $(SOURCE_FOLDER)/disk.c
	$(CC) $(CFLAGS) $< -o $@

# Compile EXT2 (C)
$(OUTPUT_FOLDER)/ext2.o: $(SOURCE_FOLDER)/ext2.c
	$(CC) $(CFLAGS) $< -o $@

# Compile string (C)
$(OUTPUT_FOLDER)/string.o: $(SOURCE_FOLDER)/string.c
	$(CC) $(CFLAGS) $< -o $@

# Compile test_ext2 (C)
$(OUTPUT_FOLDER)/test_ext2.o: $(SOURCE_FOLDER)/test_ext2.c 
	$(CC) $(CFLAGS) $< -o $@   

# Compile paging (C)
$(OUTPUT_FOLDER)/paging.o: $(SOURCE_FOLDER)/paging.c
	$(CC) $(CFLAGS) $< -o $@

# Compile process (C)
$(OUTPUT_FOLDER)/process.o: $(SOURCE_FOLDER)/process.c
	$(CC) $(CFLAGS) $< -o $@

# Compile scheduler (C)
$(OUTPUT_FOLDER)/scheduler.o: $(SOURCE_FOLDER)/scheduler.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT_FOLDER)/cmos.o: $(SOURCE_FOLDER)/cmos.c
	$(CC) $(CFLAGS) $< -o $@

# Link Semua Object Files - FIX: Use TAB instead of spaces
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
