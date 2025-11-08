#!/bin/bash
build_dir="build"
src_dir="src"
boot_src="$src_dir/boot"
kernel_src="$src_dir/kernel"
drivers_src="$src_dir/drivers"
includes_src="$src_dir/include"
img_src="$src_dir/images"
lib_src="$src_dir/lib"
echo "Building XC-OS..."

echo "Cleaning..."
if [ -d "$build_dir" ]; then
    rm -rf "$build_dir"
fi
mkdir -p "$build_dir"

echo "Assembling Stage 1..."
nasm -f bin "$boot_src/stage1.asm" -o "$build_dir/stage1.bin"
if [ $? -ne 0 ]; then
    echo "Error assembling Stage 1"
    exit 1
fi

echo "Assembling Stage 2..."
nasm -f bin "$boot_src/stage2.asm" -o "$build_dir/stage2.bin"
if [ $? -ne 0 ]; then
    echo "Error assembling Stage 2"
    exit 1
fi

echo "Assembling Kernel Boot Code..."
nasm -f elf32 $kernel_src/boot.asm -o $build_dir/boot.o
if [ $? -ne 0 ]; then
    echo "Error assembling Kernel Boot Code"
    exit 1
fi

echo "Compiling Font..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -I$src_dir/include -c $img_src/font.c -o $build_dir/font.o
if [ $? -ne 0 ]; then
    echo "Error compiling Font"
    exit 1
fi

echo "Compiling VBE Driver..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -I$src_dir/include -c $drivers_src/vbe.c -o $build_dir/vbe.o
if [ $? -ne 0 ]; then
    echo "Error compiling VBE Driver"
    exit 1
fi

echo "Compiling Memory Manager..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -I$src_dir/include -c $lib_src/memory.c -o $build_dir/memory.o
if [ $? -ne 0 ]; then
    echo "Error compiling Memory Manager"
    exit 1
fi

echo "Compiling Kernel..."
gcc -m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -I$src_dir/include -c $kernel_src/kernel.c -o $build_dir/kernel.o
if [ $? -ne 0 ]; then
    echo "Error compiling Kernel"
    exit 1
fi

echo "Linking Kernel..."
ld -m elf_i386 -T $src_dir/linker.ld -o $build_dir/kernel.bin \
    $build_dir/boot.o $build_dir/kernel.o $build_dir/vbe.o \
    $build_dir/font.o $build_dir/memory.o
if [ $? -ne 0 ]; then
    echo "Error linking Kernel"
    exit 1
fi

truncate -s 32768 "$build_dir/kernel.bin"

echo "Creating OS image..."
cat "$build_dir/stage1.bin" "$build_dir/stage2.bin" "$build_dir/kernel.bin" > "$build_dir/xcos.img"

SIZE_STAGE1=$(stat -c%s "$build_dir/stage1.bin" 2>/dev/null || stat -f%z "$build_dir/stage1.bin" 2>/dev/null)
SIZE_STAGE2=$(stat -c%s "$build_dir/stage2.bin" 2>/dev/null || stat -f%z "$build_dir/stage2.bin" 2>/dev/null)
SIZE_KERNEL=$(stat -c%s "$build_dir/kernel.bin" 2>/dev/null || stat -f%z "$build_dir/kernel.bin" 2>/dev/null)
SIZE_TOTAL=$(stat -c%s "$build_dir/xcos.img" 2>/dev/null || stat -f%z "$build_dir/xcos.img" 2>/dev/null)

echo ""
echo "Build completed successfully!"
echo "=========================="
echo "Stage 1: $SIZE_STAGE1 bytes"
echo "Stage 2: $SIZE_STAGE2 bytes"
echo "Kernel:  $SIZE_KERNEL bytes"
echo "Total:   $SIZE_TOTAL bytes"
echo "=========================="