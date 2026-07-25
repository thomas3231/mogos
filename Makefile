ASM=nasm
CC=i686-elf-gcc
LD=i686-elf-ld

SRC_DIR=src
TOOLS_DIR=tools
BUILD_DIR=build

.PHONY: all floppy_image kernel bootloader clean always tools_fat

all: floppy_image tools_fat

#
# Floppy image
#
floppy_image: $(BUILD_DIR)/main_floppy.img

$(BUILD_DIR)/main_floppy.img: bootloader kernel
	dd if=/dev/zero of=$(BUILD_DIR)/main_floppy.img bs=512 count=2880
	mkfs.fat -F 12 -n "NBOS" $(BUILD_DIR)/main_floppy.img
	dd if=$(BUILD_DIR)/bootloader.bin of=$(BUILD_DIR)/main_floppy.img conv=notrunc
	mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/kernel.bin "::kernel.bin"
	mcopy -i $(BUILD_DIR)/main_floppy.img test.txt "::test.txt"

#
# Bootloader
#
bootloader: $(BUILD_DIR)/bootloader.bin

$(BUILD_DIR)/bootloader.bin: always
	$(ASM) $(SRC_DIR)/bootloader/boot.asm -f bin -o $(BUILD_DIR)/bootloader.bin

#
# Kernel
#
kernel: $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel.bin: always $(BUILD_DIR)/kernel_entry.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/graphics.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/heap.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/palette.o $(BUILD_DIR)/window.o $(BUILD_DIR)/mouse.o
	$(LD) -m elf_i386 -T $(SRC_DIR)/kernel/linker.ld -o $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/kernel_entry.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/graphics.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/heap.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/palette.o $(BUILD_DIR)/window.o $(BUILD_DIR)/mouse.o --oformat binary

$(BUILD_DIR)/kernel_entry.o: always $(SRC_DIR)/kernel/main.asm
	$(ASM) $(SRC_DIR)/kernel/main.asm -f elf32 -o $(BUILD_DIR)/kernel_entry.o

$(BUILD_DIR)/kernel.o: always $(SRC_DIR)/kernel/kernel.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/kernel.c -o $(BUILD_DIR)/kernel.o

$(BUILD_DIR)/graphics.o: always $(SRC_DIR)/kernel/graphics.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/graphics.c -o $(BUILD_DIR)/graphics.o

$(BUILD_DIR)/keyboard.o: always $(SRC_DIR)/kernel/keyboard.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/keyboard.c -o $(BUILD_DIR)/keyboard.o
	
$(BUILD_DIR)/heap.o: always $(SRC_DIR)/kernel/heap.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/heap.c -o $(BUILD_DIR)/heap.o

$(BUILD_DIR)/timer.o: always $(SRC_DIR)/kernel/timer.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/timer.c -o $(BUILD_DIR)/timer.o

$(BUILD_DIR)/palette.o: always $(SRC_DIR)/kernel/palette.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/palette.c -o $(BUILD_DIR)/palette.o

$(BUILD_DIR)/window.o: always $(SRC_DIR)/kernel/window.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/window.c -o $(BUILD_DIR)/window.o

$(BUILD_DIR)/mouse.o: always $(SRC_DIR)/kernel/mouse.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/mouse.c -o $(BUILD_DIR)/mouse.o

#
# Tools
#
tools_fat: $(BUILD_DIR)/tools/fat
$(BUILD_DIR)/tools/fat: always $(TOOLS_DIR)/fat/fat.c
	mkdir -p $(BUILD_DIR)/tools
	gcc -std=gnu17 -g -o $(BUILD_DIR)/tools/fat $(TOOLS_DIR)/fat/fat.c

#
# Always
#
always:
	mkdir -p $(BUILD_DIR)

#
# Clean
#
clean:
	rm -rf $(BUILD_DIR)/*