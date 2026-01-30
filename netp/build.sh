#!/bin/bash

set -e

build_dir="build"
src_dir="src"
boot_src="$src_dir/boot"
kernel_src="$src_dir/kernel"
drivers_src="$src_dir/drivers"
includes_src="$src_dir/include"
lib_src="$src_dir/lib"

IMAGE_SIZE_MB=64
KERNEL_SIZE_SECTORS=255

echo "Building OpenXCOS Network Edition..."
echo "Image: ${IMAGE_SIZE_MB}MB | Kernel: ${KERNEL_SIZE_SECTORS} sectors"

if [ -d "$build_dir" ]; then
    rm -rf "$build_dir"
fi
mkdir -p "$build_dir"

nasm -f bin "$boot_src/stage1.asm" -o "$build_dir/stage1.bin"
[ $? -ne 0 ] && exit 1

nasm -f bin "$boot_src/stage2.asm" -o "$build_dir/stage2.bin"
[ $? -ne 0 ] && exit 1

nasm -f elf32 $kernel_src/boot.asm -o $build_dir/boot.o
[ $? -ne 0 ] && exit 1

gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $lib_src/string.c -o $build_dir/string.o
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $lib_src/time.c -o $build_dir/time.o
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $lib_src/random.c -o $build_dir/random.o
[ $? -ne 0 ] && exit 1

gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/graphics/vbe.c -o $build_dir/vbe.o
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/graphics/framebuffer.c -o $build_dir/framebuffer.o
[ $? -ne 0 ] && exit 1

gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/memory/pmm.c -o $build_dir/pmm.o
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/memory/vmm.c -o $build_dir/vmm.o
[ $? -ne 0 ] && exit 1

gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/text/text.c -o $build_dir/text.o
[ $? -ne 0 ] && exit 1

gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/input/keyboard.c -o $build_dir/keyboard.o
[ $? -ne 0 ] && exit 1

gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/cpu/cpu.c -o $build_dir/cpu.o
[ $? -ne 0 ] && exit 1

gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/storage/ata.c -o $build_dir/ata.o
[ $? -ne 0 ] && exit 1

nasm -f elf32 $kernel_src/interrupts/isr.asm -o $build_dir/isr.o
[ $? -ne 0 ] && exit 1

gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $kernel_src/interrupts/idt.c -o $build_dir/idt.o
[ $? -ne 0 ] && exit 1

gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/fs/xcfs.c -o $build_dir/xcfs.o
[ $? -ne 0 ] && exit 1

gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/scheduler/scheduler.c -o $build_dir/scheduler.o
[ $? -ne 0 ] && exit 1

gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/pci/pci.c -o $build_dir/pci.o
[ $? -ne 0 ] && exit 1

gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/net/rtl8139.c -o $build_dir/rtl8139.o
[ $? -ne 0 ] && exit 1

gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $drivers_src/net/network.c -o $build_dir/network.o
[ $? -ne 0 ] && exit 1

gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector -I$includes_src -c $kernel_src/kernel.c -o $build_dir/kernel.o
[ $? -ne 0 ] && exit 1

ld -m elf_i386 -T "$src_dir/linker.ld" -o "$build_dir/kernel.bin" \
    "$build_dir/boot.o" "$build_dir/kernel.o" "$build_dir/vbe.o" \
    "$build_dir/pmm.o" "$build_dir/vmm.o" "$build_dir/framebuffer.o" "$build_dir/text.o" \
    "$build_dir/string.o" "$build_dir/keyboard.o" "$build_dir/time.o" \
    "$build_dir/random.o" "$build_dir/cpu.o" "$build_dir/idt.o" \
    "$build_dir/isr.o" "$build_dir/ata.o" "$build_dir/xcfs.o" "$build_dir/scheduler.o" \
    "$build_dir/pci.o" "$build_dir/rtl8139.o" "$build_dir/network.o"
[ $? -ne 0 ] && exit 1

KERNEL_SIZE=$(($KERNEL_SIZE_SECTORS * 512))
truncate -s $KERNEL_SIZE "$build_dir/kernel.bin"

cat "$build_dir/stage1.bin" "$build_dir/stage2.bin" "$build_dir/kernel.bin" > "$build_dir/xcos.img"

IMAGE_SIZE=$(($IMAGE_SIZE_MB * 1024 * 1024))
truncate -s $IMAGE_SIZE "$build_dir/xcos.img"

SIZE_KERNEL=$(stat -c%s "$build_dir/kernel.bin" 2>/dev/null || stat -f%z "$build_dir/kernel.bin" 2>/dev/null)
SIZE_TOTAL=$(stat -c%s "$build_dir/xcos.img" 2>/dev/null || stat -f%z "$build_dir/xcos.img" 2>/dev/null)

echo ""
echo "Build completed successfully!"
echo "Kernel: $(($SIZE_KERNEL / 1024))KB | Image: ${IMAGE_SIZE_MB}MB"
