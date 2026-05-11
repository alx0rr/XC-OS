#!/bin/bash

set -e

BUILD="build"
SRC="src"
BOOT="$SRC/boot"
KERNEL="$SRC/kernel"
DRIVERS="$SRC/drivers"
INC="$SRC/include"
LIB="$SRC/lib"

IMAGE_SIZE_MB=12
SECTORS_BOOTLOADER=32
SECTORS_KERNEL=2017
XCFS_META_START=2050
XCFS_META_SECTORS=1001
XCFS_DATA_START=3051

CFLAGS="-m32 -ffreestanding -fno-pie -nostdlib -fno-builtin -fno-stack-protector \
        -O2 -march=i686 -mtune=generic \
        -fomit-frame-pointer -fno-exceptions \
        -I$INC"

NASM_ELF="-f elf32"
NASM_BIN="-f bin"

step() { echo -e "\e[1;34m→\e[0m $1"; }
ok()   { echo -e "\e[1;32m✓\e[0m $1"; }
die()  { echo -e "\e[1;31m✗\e[0m $1" >&2; exit 1; }

cc()  { gcc $CFLAGS -c "$1" -o "$2" || die "Failed: $1"; }
asm() { nasm $3 "$1" -o "$2"        || die "Failed: $1"; }

step "Cleaning..."
rm -rf "$BUILD" && mkdir -p "$BUILD"

step "Assembling bootloader..."
asm "$BOOT/stage1.asm"        "$BUILD/stage1.bin"  "$NASM_BIN"
asm "$BOOT/stage2.asm"        "$BUILD/stage2.bin"  "$NASM_BIN"

step "Assembling kernel entry..."
asm "$KERNEL/boot.asm"        "$BUILD/boot.o"      "$NASM_ELF"

step "Assembling IDT/ISR..."
asm "$KERNEL/interrupts/isr.asm" "$BUILD/isr.o"   "$NASM_ELF"

step "Compiling libs..."
cc "$LIB/string.c"            "$BUILD/string.o"
cc "$LIB/time.c"              "$BUILD/time.o"
cc "$LIB/random.c"            "$BUILD/random.o"

step "Compiling drivers..."
cc "$DRIVERS/graphics/vbe.c"          "$BUILD/vbe.o"
cc "$DRIVERS/graphics/framebuffer.c"  "$BUILD/framebuffer.o"
cc "$DRIVERS/text/text.c"             "$BUILD/text.o"
cc "$DRIVERS/memory/pmm.c"            "$BUILD/pmm.o"
cc "$DRIVERS/memory/vmm.c"            "$BUILD/vmm.o"
cc "$DRIVERS/input/keyboard.c"        "$BUILD/keyboard.o"
cc "$DRIVERS/cpu/cpu.c"               "$BUILD/cpu.o"
cc "$DRIVERS/storage/ata.c"           "$BUILD/ata.o"
cc "$DRIVERS/timer/pit.c"             "$BUILD/pit.o"
cc "$DRIVERS/sound/pcspk.c"           "$BUILD/pcspk.o"
cc "$DRIVERS/fs/xcfs.c"               "$BUILD/xcfs.o"

step "Compiling IDT..."
cc "$KERNEL/interrupts/idt.c"  "$BUILD/idt.o"

step "Compiling kernel..."
cc "$KERNEL/kernel.c"          "$BUILD/kernel.o"

step "Linking kernel..."
ld -m elf_i386 -T "$SRC/linker.ld" -o "$BUILD/kernel.bin" \
    "$BUILD/boot.o"       "$BUILD/kernel.o"    "$BUILD/idt.o"       "$BUILD/isr.o"   \
    "$BUILD/vbe.o"        "$BUILD/framebuffer.o" "$BUILD/text.o"    \
    "$BUILD/pmm.o"        "$BUILD/vmm.o"        \
    "$BUILD/string.o"     "$BUILD/time.o"       "$BUILD/random.o"   \
    "$BUILD/keyboard.o"   "$BUILD/cpu.o"        "$BUILD/ata.o"       \
    "$BUILD/xcfs.o"       "$BUILD/pit.o"        "$BUILD/pcspk.o" \
    || die "Linking failed"

step "Padding binaries..."
truncate -s $(( SECTORS_KERNEL * 512 ))    "$BUILD/kernel.bin"
truncate -s $(( 31 * 512 ))                "$BUILD/stage2.bin"

step "Building disk image (${IMAGE_SIZE_MB}MB)..."
dd if=/dev/zero of="$BUILD/xcos.img" bs=512 count=$(( IMAGE_SIZE_MB * 2048 )) 2>/dev/null

dd if="$BUILD/stage1.bin" of="$BUILD/xcos.img" bs=512 count=1                    conv=notrunc seek=0  2>/dev/null || die "Write stage1"
dd if="$BUILD/stage2.bin" of="$BUILD/xcos.img" bs=512 count=31                   conv=notrunc seek=1  2>/dev/null || die "Write stage2"
dd if="$BUILD/kernel.bin" of="$BUILD/xcos.img" bs=512 count=$SECTORS_KERNEL      conv=notrunc seek=32 2>/dev/null || die "Write kernel"

fsize() { stat -c%s "$1" 2>/dev/null || stat -f%z "$1"; }

S1=$(fsize "$BUILD/stage1.bin")
S2=$(fsize "$BUILD/stage2.bin")
SK=$(fsize "$BUILD/kernel.bin")
SI=$(fsize "$BUILD/xcos.img")

echo ""
echo "  Layout:"
printf "  %-16s %8d bytes   [sectors 0]\n"      "Stage 1:"   $S1
printf "  %-16s %8d bytes   [sectors 1–31]\n"   "Stage 2:"   $S2
printf "  %-16s %8d bytes   [sectors 32–2048]\n" "Kernel:"   $SK
printf "  %-16s %8d bytes   [sectors $XCFS_META_START–$(( XCFS_META_START + XCFS_META_SECTORS - 1 ))]\n" "XCFS metadata:" $(( XCFS_META_SECTORS * 512 ))
printf "  %-16s %8s         [sector $XCFS_DATA_START+]\n"   "XCFS data:"    "—"
echo ""
printf "  %-16s %8d bytes   (%d MB)\n"           "Image total:"  $SI $IMAGE_SIZE_MB
echo ""
ok "Done → $BUILD/xcos.img"

