#!/bin/bash

build_dir="build"
src_dir="src"
boot_src="$src_dir/boot"
kernel_src="$src_dir/kernel"
drivers_src="$src_dir/drivers"
includes_src="$src_dir/include"
lib_src="$src_dir/lib"

IMAGE_SIZE_MB=12
BOOTLOADER_SECTORS=32
KERNEL_SECTORS=2017
XCFS_METADATA_START=2050
XCFS_METADATA_SECTORS=1001
XCFS_DATA_START=3051

echo "Building XC-OS..."
echo "Image size: ${IMAGE_SIZE_MB}MB"
echo "Bootloader: ${BOOTLOADER_SECTORS} sectors ($(($BOOTLOADER_SECTORS * 512 / 1024))KB) [sectors 0-31]"
echo "Kernel: ${KERNEL_SECTORS} sectors ($(($KERNEL_SECTORS * 512 / 1024))KB) [sectors 32-2048]"
echo "XCFS v2 metadata: ${XCFS_METADATA_SECTORS} sectors [sectors 2050-3050]"
echo "XCFS data: starts at sector ${XCFS_DATA_START}"
echo ""

echo "Cleaning..."
if [ -d "$build_dir" ]; then
    rm -rf "$build_dir"
fi
mkdir -p "$build_dir"

echo "Assembling Stage 1..."
nasm -f bin "$boot_src/stage1.asm" -o "$build_dir/stage1.bin"
[ $? -ne 0 ] && echo "Error assembling Stage 1" && exit 1

echo "Assembling Stage 2..."
nasm -f bin "$boot_src/stage2.asm" -o "$build_dir/stage2.bin"
[ $? -ne 0 ] && echo "Error assembling Stage 2" && exit 1

echo "Assembling Kernel Boot Code..."
nasm -f elf32 $kernel_src/boot.asm -o $build_dir/boot.o
[ $? -ne 0 ] && echo "Error assembling Kernel Boot Code" && exit 1

echo "Compiling libs..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $lib_src/string.c -o $build_dir/string.o
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $lib_src/time.c -o $build_dir/time.o
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $lib_src/random.c -o $build_dir/random.o
[ $? -ne 0 ] && echo "Error compiling libs" && exit 1

echo "Compiling VBE Driver..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/graphics/vbe.c -o $build_dir/vbe.o
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/graphics/framebuffer.c -o $build_dir/framebuffer.o
[ $? -ne 0 ] && echo "Error compiling VBE Driver" && exit 1

echo "Compiling Memory Manager..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/memory/pmm.c -o $build_dir/pmm.o
[ $? -ne 0 ] && echo "Error compiling Memory Manager" && exit 1

echo "Compiling Virtual Memory Manager..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/memory/vmm.c -o $build_dir/vmm.o
[ $? -ne 0 ] && echo "Error compiling Virtual Memory Manager" && exit 1

echo "Compiling Text Manager..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/text/text.c -o $build_dir/text.o
[ $? -ne 0 ] && echo "Error compiling Text Manager" && exit 1

echo "Compiling Keyboard Driver..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/input/keyboard.c -o $build_dir/keyboard.o
[ $? -ne 0 ] && echo "Error compiling Keyboard Driver" && exit 1

echo "Compiling CPU Driver..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/cpu/cpu.c -o $build_dir/cpu.o
[ $? -ne 0 ] && echo "Error compiling CPU Driver" && exit 1

echo "Compiling ATA Driver..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/storage/ata.c -o $build_dir/ata.o
[ $? -ne 0 ] && echo "Error compiling ATA Driver" && exit 1

echo "Compiling PIT Driver..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/timer/pit.c -o $build_dir/pit.o
[ $? -ne 0 ] && echo "Error compiling PIT Driver" && exit 1

echo "Compiling PC Speaker Driver..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/sound/pcspk.c -o $build_dir/pcspk.o
[ $? -ne 0 ] && echo "Error compiling PC Speaker Driver" && exit 1

echo "Assembling IDT/ISR..."
nasm -f elf32 $kernel_src/interrupts/isr.asm -o $build_dir/isr.o
[ $? -ne 0 ] && echo "Error assembling ISR" && exit 1

echo "Compiling IDT..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $kernel_src/interrupts/idt.c -o $build_dir/idt.o
[ $? -ne 0 ] && echo "Error compiling IDT" && exit 1

echo "Compiling XCFS v2..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/fs/xcfs.c -o $build_dir/xcfs.o
[ $? -ne 0 ] && echo "Error compiling XCFS" && exit 1

echo "Compiling Task Scheduler..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/scheduler/scheduler.c -o $build_dir/scheduler.o
[ $? -ne 0 ] && echo "Error compiling Scheduler" && exit 1

echo "Compiling Kernel..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $kernel_src/kernel.c -o $build_dir/kernel.o
[ $? -ne 0 ] && echo "Error compiling Kernel" && exit 1

echo "Linking Kernel..."
ld -m elf_i386 -T "$src_dir/linker.ld" -o "$build_dir/kernel.bin" \
    "$build_dir/boot.o" "$build_dir/kernel.o" "$build_dir/vbe.o" \
    "$build_dir/pmm.o" "$build_dir/vmm.o" "$build_dir/framebuffer.o" "$build_dir/text.o" \
    "$build_dir/string.o" "$build_dir/keyboard.o" "$build_dir/time.o" \
    "$build_dir/random.o" "$build_dir/cpu.o" "$build_dir/idt.o" \
    "$build_dir/isr.o" "$build_dir/ata.o" "$build_dir/xcfs.o" "$build_dir/scheduler.o" "$build_dir/pit.o" "$build_dir/pcspk.o"
[ $? -ne 0 ] && echo "Error linking Kernel" && exit 1

KERNEL_SIZE=$(($KERNEL_SECTORS * 512))
truncate -s $KERNEL_SIZE "$build_dir/kernel.bin"

STAGE2_SIZE=$((31 * 512))
truncate -s $STAGE2_SIZE "$build_dir/stage2.bin"

echo "Creating disk image with updated layout..."

IMAGE_SIZE=$(($IMAGE_SIZE_MB * 1024 * 1024))
dd if=/dev/zero of="$build_dir/xcos.img" bs=512 count=$(($IMAGE_SIZE / 512)) 2>/dev/null

dd if="$build_dir/stage1.bin" of="$build_dir/xcos.img" bs=512 count=1 conv=notrunc seek=0 2>/dev/null
[ $? -ne 0 ] && echo "Error writing Stage 1" && exit 1

dd if="$build_dir/stage2.bin" of="$build_dir/xcos.img" bs=512 count=31 conv=notrunc seek=1 2>/dev/null
[ $? -ne 0 ] && echo "Error writing Stage 2" && exit 1

dd if="$build_dir/kernel.bin" of="$build_dir/xcos.img" bs=512 count=$KERNEL_SECTORS conv=notrunc seek=32 2>/dev/null
[ $? -ne 0 ] && echo "Error writing Kernel" && exit 1

SIZE_STAGE1=$(stat -c%s "$build_dir/stage1.bin" 2>/dev/null || stat -f%z "$build_dir/stage1.bin" 2>/dev/null)
SIZE_STAGE2=$(stat -c%s "$build_dir/stage2.bin" 2>/dev/null || stat -f%z "$build_dir/stage2.bin" 2>/dev/null)
SIZE_KERNEL=$(stat -c%s "$build_dir/kernel.bin" 2>/dev/null || stat -f%z "$build_dir/kernel.bin" 2>/dev/null)
SIZE_TOTAL=$(stat -c%s "$build_dir/xcos.img" 2>/dev/null || stat -f%z "$build_dir/xcos.img" 2>/dev/null)

echo ""
echo "Stage 1:        $SIZE_STAGE1 bytes"
echo "Stage 2:        $SIZE_STAGE2 bytes"
echo "Kernel:         $SIZE_KERNEL bytes"
echo "Total Image:    $SIZE_TOTAL bytes (${IMAGE_SIZE_MB} MB)"
echo "Output: $build_dir/xcos.img"

