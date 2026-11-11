BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso

CXX := g++
AS := nasm
LD := ld

CXXFLAGS := -m32 -std=c++17 -ffreestanding -fno-exceptions -fno-rtti \
            -fno-stack-protector -fno-pic -fno-pie -fno-use-cxa-atexit \
            -fno-threadsafe-statics -Wall -Wextra -O2
LDFLAGS := -m elf_i386 -T linker.ld

.PHONY: all clean run check

all: $(BUILD_DIR)/a2os.iso

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot.o: boot/boot.asm | $(BUILD_DIR)
	$(AS) -f elf32 $< -o $@

$(BUILD_DIR)/kernel.o: kernel/kernel.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.elf: $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o linker.ld
	$(LD) $(LDFLAGS) -o $@ $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o

$(BUILD_DIR)/a2os.iso: $(BUILD_DIR)/kernel.elf grub/grub.cfg
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(BUILD_DIR)/kernel.elf $(ISO_DIR)/boot/kernel.elf
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_DIR)

check: $(BUILD_DIR)/kernel.elf
	grub-file --is-x86-multiboot $(BUILD_DIR)/kernel.elf

run: $(BUILD_DIR)/a2os.iso
	qemu-system-i386 -cdrom $(BUILD_DIR)/a2os.iso -m 128M -device isa-debug-exit,iobase=0xf4,iosize=0x04

clean:
	rm -rf $(BUILD_DIR)
