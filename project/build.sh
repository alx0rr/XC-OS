#!/bin/bash

build_dir="build"
src_dir="src"
boot_src="$src_dir/boot"
kernel_src="$src_dir/kernel"
drivers_src="$src_dir/drivers"
includes_src="$src_dir/include"
lib_src="$src_dir/lib"

IMAGE_SIZE_MB=64
KERNEL_SIZE_SECTORS=255

echo "Building XC-OS v2.0..."
echo "Image size: ${IMAGE_SIZE_MB}MB"
echo "Kernel size: ${KERNEL_SIZE_SECTORS} sectors ($(($KERNEL_SIZE_SECTORS * 512 / 1024))KB)"

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

echo "Assembling IDT/ISR..."
nasm -f elf32 $kernel_src/interrupts/isr.asm -o $build_dir/isr.o
[ $? -ne 0 ] && echo "Error assembling ISR" && exit 1

echo "Compiling IDT..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $kernel_src/interrupts/idt.c -o $build_dir/idt.o
[ $? -ne 0 ] && echo "Error compiling IDT" && exit 1

echo "Compiling XCFS v2..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/fs/xcfs.c -o $build_dir/xcfs.o
[ $? -ne 0 ] && echo "Error compiling XCFS" && exit 1

echo "Compiling Kernel..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $kernel_src/kernel.c -o $build_dir/kernel.o
[ $? -ne 0 ] && echo "Error compiling Kernel" && exit 1

echo "Linking Kernel..."
ld -m elf_i386 -T "$src_dir/linker.ld" -o "$build_dir/kernel.bin" \
    "$build_dir/boot.o" "$build_dir/kernel.o" "$build_dir/vbe.o" \
    "$build_dir/pmm.o" "$build_dir/framebuffer.o" "$build_dir/text.o" \
    "$build_dir/string.o" "$build_dir/keyboard.o" "$build_dir/time.o" \
    "$build_dir/random.o" "$build_dir/cpu.o" "$build_dir/idt.o" \
    "$build_dir/isr.o" "$build_dir/ata.o" "$build_dir/xcfs.o"
[ $? -ne 0 ] && echo "Error linking Kernel" && exit 1

KERNEL_SIZE=$(($KERNEL_SIZE_SECTORS * 512))
truncate -s $KERNEL_SIZE "$build_dir/kernel.bin"

echo "Creating OS image..."
cat "$build_dir/stage1.bin" "$build_dir/stage2.bin" "$build_dir/kernel.bin" > "$build_dir/xcos.img"

IMAGE_SIZE=$(($IMAGE_SIZE_MB * 1024 * 1024))
truncate -s $IMAGE_SIZE "$build_dir/xcos.img"

SIZE_STAGE1=$(stat -c%s "$build_dir/stage1.bin" 2>/dev/null || stat -f%z "$build_dir/stage1.bin" 2>/dev/null)
SIZE_STAGE2=$(stat -c%s "$build_dir/stage2.bin" 2>/dev/null || stat -f%z "$build_dir/stage2.bin" 2>/dev/null)
SIZE_KERNEL=$(stat -c%s "$build_dir/kernel.bin" 2>/dev/null || stat -f%z "$build_dir/kernel.bin" 2>/dev/null)
SIZE_TOTAL=$(stat -c%s "$build_dir/xcos.img" 2>/dev/null || stat -f%z "$build_dir/xcos.img" 2>/dev/null)

echo ""
echo "Build completed successfully!"
echo "=========================="
echo "Stage 1: $SIZE_STAGE1 bytes"
echo "Stage 2: $SIZE_STAGE2 bytes"
echo "Kernel:  $SIZE_KERNEL bytes ($(($SIZE_KERNEL / 1024))KB)"
echo "Image:   $SIZE_TOTAL bytes (${IMAGE_SIZE_MB} MB)"
echo "=========================="

