#!/bin/bash
set -e

CFG="config.cfg"
[ -f "$CFG" ] || { echo "config.cfg not found"; exit 1; }
source "$CFG"

BUILD="bld"
SRC="src"
BOOT="$SRC/boot"
KERNEL="$SRC/kernel"
DRIVERS="$SRC/drivers"
INC="$SRC/include"
LIB="$SRC/lib"

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
na()  { nasm $3 "$1" -o "$2"        || die "Failed: $1"; }

step "Cleaning..."
rm -rf "$BUILD" && mkdir -p "$BUILD"

step "Generating config headers..."

cat > "$INC/config.h" <<EOF
#ifndef CONFIG_H
#define CONFIG_H
#define CFG_IMAGE_SIZE_MB       $IMAGE_SIZE_MB
#define CFG_KERNEL_START_SECTOR $KERNEL_START_SECTOR
#define CFG_KERNEL_MAX_SECTORS  $KERNEL_MAX_SECTORS
#define CFG_XCFS_META_SECTOR    $XCFS_META_SECTOR
#define CFG_XCFS_META_SECTORS   $XCFS_META_SECTORS
#define CFG_XCFS_DATA_SECTOR    $XCFS_DATA_SECTOR
#endif
EOF

cat > "$BOOT/config.inc" <<EOF
%define CFG_KERNEL_START_SECTOR $KERNEL_START_SECTOR
%define CFG_KERNEL_MAX_SECTORS  $KERNEL_MAX_SECTORS
%define CFG_XCFS_META_SECTOR    $XCFS_META_SECTOR
%define CFG_XCFS_DATA_SECTOR    $XCFS_DATA_SECTOR
EOF

step "Assembling bootloader..."
na "$BOOT/stage1.asm"            "$BUILD/stage1.bin" "$NASM_BIN"
na "$BOOT/stage2.asm"            "$BUILD/stage2.bin" "$NASM_BIN"

step "Assembling kernel entry..."
na "$KERNEL/boot.asm"            "$BUILD/boot.o"     "$NASM_ELF"

step "Assembling IDT/ISR..."
na "$KERNEL/interrupts/isr.asm"  "$BUILD/isr.o"      "$NASM_ELF"

step "Compiling libs..."
cc "$LIB/string.c"               "$BUILD/string.o"
cc "$LIB/time.c"                 "$BUILD/time.o"
cc "$LIB/random.c"               "$BUILD/random.o"

step "Compiling drivers..."
cc "$DRIVERS/graphics/vbe.c"         "$BUILD/vbe.o"
cc "$DRIVERS/graphics/framebuffer.c" "$BUILD/framebuffer.o"
cc "$DRIVERS/text/text.c"            "$BUILD/text.o"
cc "$DRIVERS/memory/pmm.c"           "$BUILD/pmm.o"
cc "$DRIVERS/memory/vmm.c"           "$BUILD/vmm.o"
cc "$DRIVERS/input/keyboard.c"       "$BUILD/keyboard.o"
cc "$DRIVERS/cpu/cpu.c"              "$BUILD/cpu.o"
cc "$DRIVERS/storage/ata.c"          "$BUILD/ata.o"
cc "$DRIVERS/timer/pit.c"            "$BUILD/pit.o"
cc "$DRIVERS/sound/pcspk.c"          "$BUILD/pcspk.o"
cc "$DRIVERS/fs/xcfs.c"              "$BUILD/xcfs.o"
cc "$DRIVERS/net/ne2000.c"           "$BUILD/ne2000.o"
cc "$DRIVERS/net/eth.c"              "$BUILD/eth.o"
cc "$DRIVERS/net/arp.c"              "$BUILD/arp.o"
cc "$DRIVERS/net/ip.c"               "$BUILD/ip.o"
cc "$DRIVERS/net/icmp.c"             "$BUILD/icmp.o"
cc "$DRIVERS/net/udp.c"              "$BUILD/udp.o"
cc "$DRIVERS/net/dns.c"              "$BUILD/dns.o"
cc "$DRIVERS/net/tcp.c"              "$BUILD/tcp.o"
cc "$DRIVERS/net/http.c"             "$BUILD/http.o"

step "Compiling IDT..."
cc "$KERNEL/interrupts/idt.c"    "$BUILD/idt.o"

step "Compiling kernel..."
cc "$KERNEL/kernel.c"            "$BUILD/kernel.o"

step "Linking kernel..."
ld -m elf_i386 -T "$SRC/linker.ld" -o "$BUILD/kernel.bin" \
    "$BUILD/boot.o"       "$BUILD/kernel.o"      "$BUILD/idt.o"      "$BUILD/isr.o"  \
    "$BUILD/vbe.o"        "$BUILD/framebuffer.o" "$BUILD/text.o"                     \
    "$BUILD/pmm.o"        "$BUILD/vmm.o"                                             \
    "$BUILD/string.o"     "$BUILD/time.o"        "$BUILD/random.o"                   \
    "$BUILD/keyboard.o"   "$BUILD/cpu.o"         "$BUILD/ata.o"                      \
    "$BUILD/xcfs.o"       "$BUILD/pit.o"         "$BUILD/pcspk.o"                    \
    "$BUILD/ne2000.o"     "$BUILD/eth.o"         "$BUILD/arp.o"                      \
    "$BUILD/ip.o"         "$BUILD/icmp.o"        "$BUILD/udp.o"                      \
    "$BUILD/dns.o"        "$BUILD/tcp.o"         "$BUILD/http.o"                     \
    || die "Linking failed"

KSIZE=$(stat -c%s "$BUILD/kernel.bin" 2>/dev/null || stat -f%z "$BUILD/kernel.bin")
KMAX=$(( KERNEL_MAX_SECTORS * 512 ))
[ "$KSIZE" -le "$KMAX" ] || die "kernel.bin ($KSIZE bytes) exceeds max ($KMAX bytes = ${KERNEL_MAX_SECTORS} sectors)"

step "Padding binaries..."
truncate -s $(( KERNEL_MAX_SECTORS * 512 )) "$BUILD/kernel.bin"
truncate -s $(( BOOT_STAGE2_COUNT * 512 ))  "$BUILD/stage2.bin"

step "Building disk image (${IMAGE_SIZE_MB}MB)..."
dd if=/dev/zero of="$BUILD/xcos.img" bs=512 count=$(( IMAGE_SIZE_MB * 2048 )) 2>/dev/null

dd if="$BUILD/stage1.bin" of="$BUILD/xcos.img" bs=512 count=$BOOT_STAGE1_COUNT conv=notrunc seek=$BOOT_STAGE1_SECTOR 2>/dev/null || die "Write stage1"
dd if="$BUILD/stage2.bin" of="$BUILD/xcos.img" bs=512 count=$BOOT_STAGE2_COUNT conv=notrunc seek=$BOOT_STAGE2_SECTOR 2>/dev/null || die "Write stage2"
dd if="$BUILD/kernel.bin" of="$BUILD/xcos.img" bs=512 count=$KERNEL_MAX_SECTORS conv=notrunc seek=$KERNEL_START_SECTOR 2>/dev/null || die "Write kernel"

fsize() { stat -c%s "$1" 2>/dev/null || stat -f%z "$1"; }
SI=$(fsize "$BUILD/xcos.img")

echo ""
echo "  Layout:"
printf "  %-18s [sector %d]\n"           "Stage 1:"          $BOOT_STAGE1_SECTOR
printf "  %-18s [sectors %d–%d]\n"       "Stage 2:"          $BOOT_STAGE2_SECTOR $(( BOOT_STAGE2_SECTOR + BOOT_STAGE2_COUNT - 1 ))
printf "  %-18s [sectors %d–%d]\n"       "Kernel (max 4MB):" $KERNEL_START_SECTOR $(( KERNEL_START_SECTOR + KERNEL_MAX_SECTORS - 1 ))
printf "  %-18s [sectors %d–%d]\n"       "XCFS metadata:"    $XCFS_META_SECTOR    $(( XCFS_META_SECTOR + XCFS_META_SECTORS - 1 ))
printf "  %-18s [sector %d+]\n"          "XCFS data:"        $XCFS_DATA_SECTOR
echo ""
printf "  %-18s %d bytes (%d MB)\n"      "Image total:"      $SI $IMAGE_SIZE_MB
echo ""
ok "Done → $BUILD/xcos.img"
