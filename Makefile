ASM=nasm
CC=i686-elf-gcc
LD=i686-elf-ld

ifeq ($(shell command -v $(CC) >/dev/null 2>&1 && command -v $(LD) >/dev/null 2>&1 && echo yes),)
$(info i686-elf toolchain not found; using native gcc/ld with -m32.)
CC=gcc
LD=ld
endif

SRC_DIR=src
TOOLS_DIR=tools
BUILD_DIR=build
XORRISO=xorriso
WINDOWS_QEMU?=/mnt/c/Program Files/qemu/qemu-system-i386.exe

.PHONY: all floppy_image iso run_iso kernel bootloader clean always tools_fat test

all: floppy_image $(BUILD_DIR)/hdd.img tools_fat


$(BUILD_DIR)/hdd.img: | always
	dd if=/dev/zero of=$(BUILD_DIR)/hdd.img bs=1M count=16


#
# Floppy image
#
floppy_image: $(BUILD_DIR)/main_floppy.img

$(BUILD_DIR)/main_floppy.img: bootloader kernel
	dd if=/dev/zero of=$(BUILD_DIR)/main_floppy.img bs=512 count=2880
	mkfs.fat -F 12 -n "NBOS" $(BUILD_DIR)/main_floppy.img
	dd if=$(BUILD_DIR)/bootloader.bin of=$(BUILD_DIR)/main_floppy.img conv=notrunc
	mcopy -i $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/kernel.bin "::kernel.bin"

#
# Legacy-BIOS El Torito ISO (floppy emulation)
#
iso: $(BUILD_DIR)/mogos.iso

$(BUILD_DIR)/mogos.iso: $(BUILD_DIR)/main_floppy.img
	command -v $(XORRISO) >/dev/null || { echo "Missing $(XORRISO). Install it with: sudo apt install xorriso"; exit 1; }
	mkdir -p $(BUILD_DIR)/iso/boot
	cp $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/iso/boot/mogos-floppy.img
	$(XORRISO) -as mkisofs -R -J -V MOGOS \
		-b boot/mogos-floppy.img -c boot/boot.cat \
		-o $@ $(BUILD_DIR)/iso

run_iso: iso $(BUILD_DIR)/hdd.img
	@if [ -x "$(WINDOWS_QEMU)" ] && grep -qi microsoft /proc/sys/kernel/osrelease; then \
		echo "Launching native Windows QEMU to avoid a transparent WSLg window."; \
		"$(WINDOWS_QEMU)" -cdrom "$$(wslpath -w $(BUILD_DIR)/mogos.iso)" -boot d \
			-display sdl -vga std \
			-drive file="$$(wslpath -w $(BUILD_DIR)/hdd.img)",format=raw,if=ide,index=0,media=disk \
			-audiodev dsound,id=snd0 -machine pcspk-audiodev=snd0; \
	else \
		qemu-system-i386 -cdrom $(BUILD_DIR)/mogos.iso -boot d \
			-display gtk,gl=off -vga std \
			-drive file=$(BUILD_DIR)/hdd.img,format=raw,if=ide,index=0,media=disk \
			-audiodev pa,id=snd0 -machine pcspk-audiodev=snd0; \
	fi

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

$(BUILD_DIR)/kernel.bin: always $(BUILD_DIR)/kernel_entry.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/graphics.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/heap.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/palette.o $(BUILD_DIR)/exceptions.o $(BUILD_DIR)/rtc.o $(BUILD_DIR)/speaker.o $(BUILD_DIR)/ata.o $(BUILD_DIR)/floppy.o $(BUILD_DIR)/desktop.o $(BUILD_DIR)/splash.o $(BUILD_DIR)/sprite.o $(BUILD_DIR)/snake.o
	$(LD) -m elf_i386 -T $(SRC_DIR)/kernel/linker.ld -o $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/kernel_entry.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/graphics.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/heap.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/palette.o $(BUILD_DIR)/exceptions.o $(BUILD_DIR)/rtc.o $(BUILD_DIR)/speaker.o $(BUILD_DIR)/ata.o $(BUILD_DIR)/floppy.o $(BUILD_DIR)/desktop.o $(BUILD_DIR)/splash.o $(BUILD_DIR)/sprite.o $(BUILD_DIR)/snake.o --oformat binary

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

$(BUILD_DIR)/exceptions.o: always $(SRC_DIR)/kernel/exceptions.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/exceptions.c -o $(BUILD_DIR)/exceptions.o

$(BUILD_DIR)/rtc.o: always $(SRC_DIR)/kernel/rtc.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/rtc.c -o $(BUILD_DIR)/rtc.o

$(BUILD_DIR)/speaker.o: always $(SRC_DIR)/kernel/speaker.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/speaker.c -o $(BUILD_DIR)/speaker.o

$(BUILD_DIR)/ata.o: always $(SRC_DIR)/kernel/ata.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/ata.c -o $(BUILD_DIR)/ata.o

$(BUILD_DIR)/floppy.o: always $(SRC_DIR)/kernel/floppy.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/floppy.c -o $(BUILD_DIR)/floppy.o

$(BUILD_DIR)/desktop.o: always $(SRC_DIR)/kernel/desktop.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/desktop.c -o $(BUILD_DIR)/desktop.o

$(BUILD_DIR)/splash.o: always $(SRC_DIR)/kernel/splash.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/splash.c -o $(BUILD_DIR)/splash.o

$(BUILD_DIR)/sprite.o: always $(SRC_DIR)/kernel/sprite.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/sprite.c -o $(BUILD_DIR)/sprite.o

$(BUILD_DIR)/snake.o: always $(SRC_DIR)/kernel/snake.c
	$(CC) -m32 -ffreestanding -fno-pie -fno-pic -c $(SRC_DIR)/kernel/snake.c -o $(BUILD_DIR)/snake.o

#
# Tools
#
tools_fat: $(BUILD_DIR)/tools/fat
$(BUILD_DIR)/tools/fat: always $(TOOLS_DIR)/fat/fat.c
	mkdir -p $(BUILD_DIR)/tools
	gcc -std=gnu17 -g -o $(BUILD_DIR)/tools/fat $(TOOLS_DIR)/fat/fat.c

test: $(BUILD_DIR)/tools/stability-test
	$(BUILD_DIR)/tools/stability-test

$(BUILD_DIR)/tools/stability-test: tests/stability_test.c $(SRC_DIR)/kernel/rtc.c $(SRC_DIR)/kernel/timer.c $(SRC_DIR)/kernel/snake.c $(SRC_DIR)/kernel/ata.c $(SRC_DIR)/kernel/desktop.c
	mkdir -p $(BUILD_DIR)/tools
	gcc -std=gnu17 -Wall -Wextra -Wno-pointer-to-int-cast -ffunction-sections -fdata-sections -I$(SRC_DIR)/kernel -Wl,--gc-sections -o $@ $^

#
# Always
#
always:
	mkdir -p $(BUILD_DIR)

#
# Clean
#
clean:
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.bin $(BUILD_DIR)/main_floppy.img $(BUILD_DIR)/mogos.iso
	rm -rf $(BUILD_DIR)/iso
	rm -rf $(BUILD_DIR)/tools