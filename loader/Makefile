CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy

CFLAGS  := -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
           -mgeneral-regs-only -Wall -Wextra -O2 -std=c11 -c
LDFLAGS := -T linker.ld -nostdlib -static

SRC_C   := src/main.c src/fdt.c
SRC_S   := src/boot.S
OBJ     := build/boot.o build/main.o build/fdt.o

.PHONY: all clean

all: build/bootloader.bin

build:
	mkdir -p build

build/boot.o: src/boot.S | build
	$(CC) $(CFLAGS) -x assembler-with-cpp $< -o $@

build/main.o: src/main.c src/fdt.h | build
	$(CC) $(CFLAGS) $< -o $@

build/fdt.o: src/fdt.c src/fdt.h | build
	$(CC) $(CFLAGS) $< -o $@

build/bootloader.elf: $(OBJ) linker.ld
	$(LD) $(LDFLAGS) $(OBJ) -o $@

build/bootloader.bin: build/bootloader.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf build
