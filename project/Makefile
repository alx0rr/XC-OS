CC = gcc
AS = nasm
LD = ld

CFLAGS = -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$(INC_DIR) -c
ASFLAGS_BIN = -f bin
ASFLAGS_ELF = -f elf32
LDFLAGS = -m elf_i386 -T $(SRC_DIR)/linker.ld

BUILD_DIR = build
SRC_DIR = src
BOOT_DIR = $(SRC_DIR)/boot
KERNEL_DIR = $(SRC_DIR)/kernel
DRIVERS_DIR = $(SRC_DIR)/drivers
INC_DIR = $(SRC_DIR)/include
LIB_DIR = $(SRC_DIR)/lib

OBJ = $(BUILD_DIR)/boot.o \
      $(BUILD_DIR)/kernel.o \
      $(BUILD_DIR)/vbe.o \
      $(BUILD_DIR)/pmm.o \
      $(BUILD_DIR)/framebuffer.o \
      $(BUILD_DIR)/text.o \
      $(BUILD_DIR)/string.o \
      $(BUILD_DIR)/keyboard.o \
      $(BUILD_DIR)/time.o \
      $(BUILD_DIR)/random.o \
      $(BUILD_DIR)/cpu.o \
      $(BUILD_DIR)/idt.o \
      $(BUILD_DIR)/isr.o

all: $(BUILD_DIR)/xcos.img

$(BUILD_DIR)/xcos.img: $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/kernel.bin
	cat $^ > $@
	@echo "Build successful. Total size:"
	@stat -c%s $@ 2>/dev/null || stat -f%z $@

$(BUILD_DIR)/stage1.bin: $(BOOT_DIR)/stage1.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS_BIN) $< -o $@

$(BUILD_DIR)/stage2.bin: $(BOOT_DIR)/stage2.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS_BIN) $< -o $@

$(BUILD_DIR)/kernel.bin: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)
	truncate -s 32768 $@

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.asm
	$(AS) $(ASFLAGS_ELF) $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/interrupts/%.asm
	$(AS) $(ASFLAGS_ELF) $< -o $@

$(BUILD_DIR)/%.o: $(KERNEL_DIR)/interrupts/%.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(LIB_DIR)/%.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(DRIVERS_DIR)/graphics/%.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(DRIVERS_DIR)/memory/%.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(DRIVERS_DIR)/text/%.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(DRIVERS_DIR)/input/%.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(DRIVERS_DIR)/cpu/%.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
