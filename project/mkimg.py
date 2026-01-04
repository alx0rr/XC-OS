#!/usr/bin/env python3
import os
import struct
import sys

SECTOR_SIZE = 512
XCFS_MAGIC = 0x58434653
XCFS_VERSION = 1
BOOTLOADER_END = 80
XCFS_START = 100
XCFS_DATA_START = 248
IMAGE_SIZE = 16 * 1024 * 1024

def create_disk_image(output_path, stage1_path, stage2_path, kernel_path):
    print("Creating XC-OS disk image...")
    
    with open(stage1_path, 'rb') as f:
        stage1 = f.read()
    print(f"Stage 1: {len(stage1)} bytes")
    
    with open(stage2_path, 'rb') as f:
        stage2 = f.read()
    print(f"Stage 2: {len(stage2)} bytes")
    
    with open(kernel_path, 'rb') as f:
        kernel = f.read()
    print(f"Kernel: {len(kernel)} bytes")
    
    if len(stage1) > SECTOR_SIZE:
        print(f"ERROR: Stage 1 too large ({len(stage1)} > {SECTOR_SIZE})")
        sys.exit(1)
    
    if len(stage2) > SECTOR_SIZE * 15:
        print(f"ERROR: Stage 2 too large ({len(stage2)} > {SECTOR_SIZE * 15})")
        sys.exit(1)
    
    if len(kernel) > SECTOR_SIZE * 64:
        print(f"ERROR: Kernel too large ({len(kernel)} > {SECTOR_SIZE * 64})")
        sys.exit(1)
    
    image = bytearray(IMAGE_SIZE)
    
    image[0:len(stage1)] = stage1
    
    stage2_start = SECTOR_SIZE
    image[stage2_start:stage2_start + len(stage2)] = stage2
    
    kernel_start = SECTOR_SIZE * 16
    image[kernel_start:kernel_start + len(kernel)] = kernel
    
    print(f"Bootloader: sectors 0-{BOOTLOADER_END - 1}")
    print(f"XCFS metadata: sectors {XCFS_START}-{XCFS_DATA_START - 1}")
    print(f"XCFS data: sector {XCFS_DATA_START}+")
    
    return image

def format_xcfs(image):
    print("\nFormatting XCFS...")
    
    total_sectors = IMAGE_SIZE // SECTOR_SIZE
    
    header = struct.pack('<IIII', XCFS_MAGIC, XCFS_VERSION, total_sectors, 0)
    header += b'\x00' * (SECTOR_SIZE - len(header))
    
    xcfs_header_offset = XCFS_START * SECTOR_SIZE
    image[xcfs_header_offset:xcfs_header_offset + SECTOR_SIZE] = header
    
    for i in range(XCFS_START + 1, XCFS_DATA_START):
        offset = i * SECTOR_SIZE
        image[offset:offset + SECTOR_SIZE] = b'\x00' * SECTOR_SIZE
    
    print(f"XCFS header at sector {XCFS_START}")
    print(f"XCFS file entries: sectors {XCFS_START + 1}-{XCFS_DATA_START - 1}")
    print(f"XCFS data area: sector {XCFS_DATA_START}+")

def add_file_to_xcfs(image, filename, content):
    print(f"\nAdding file: {filename}")
    
    header_offset = XCFS_START * SECTOR_SIZE
    magic, version, total_sectors, file_count = struct.unpack('<IIII', image[header_offset:header_offset + 16])
    
    if file_count >= 256:
        print("ERROR: Maximum file count reached (256)")
        return False
    
    content_bytes = content.encode('utf-8') if isinstance(content, str) else content
    file_size = len(content_bytes)
    
    start_sector = XCFS_DATA_START
    for i in range(file_count):
        entry_offset = (XCFS_START + 1) * SECTOR_SIZE + (i * 64)
        entry_data = image[entry_offset:entry_offset + 64]
        
        entry_start = struct.unpack('<I', entry_data[56:60])[0]
        entry_size = struct.unpack('<I', entry_data[60:64])[0]
        entry_end = entry_start + ((entry_size + SECTOR_SIZE - 1) // SECTOR_SIZE)
        
        if entry_end > start_sector:
            start_sector = entry_end
    
    filename_bytes = filename.encode('utf-8')
    if len(filename_bytes) > 56:
        filename_bytes = filename_bytes[:56]
    
    entry = bytearray(64)
    entry[0:len(filename_bytes)] = filename_bytes
    entry[56:60] = struct.pack('<I', start_sector)
    entry[60:64] = struct.pack('<I', file_size)
    
    entry_offset = (XCFS_START + 1) * SECTOR_SIZE + (file_count * 64)
    image[entry_offset:entry_offset + 64] = entry
    
    data_offset = start_sector * SECTOR_SIZE
    image[data_offset:data_offset + file_size] = content_bytes
    
    file_count += 1
    image[header_offset + 12:header_offset + 16] = struct.pack('<I', file_count)
    
    sectors_used = (file_size + SECTOR_SIZE - 1) // SECTOR_SIZE
    print(f"  Name: {filename}")
    print(f"  Size: {file_size} bytes")
    print(f"  Start sector: {start_sector}")
    print(f"  Sectors used: {sectors_used}")
    
    return True

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 mkimg.py <output_image>")
        sys.exit(1)
    
    output_image = sys.argv[1]
    
    build_dir = "build"
    stage1_path = os.path.join(build_dir, "stage1.bin")
    stage2_path = os.path.join(build_dir, "stage2.bin")
    kernel_path = os.path.join(build_dir, "kernel.bin")
    
    if not os.path.exists(stage1_path):
        print(f"ERROR: {stage1_path} not found")
        sys.exit(1)
    if not os.path.exists(stage2_path):
        print(f"ERROR: {stage2_path} not found")
        sys.exit(1)
    if not os.path.exists(kernel_path):
        print(f"ERROR: {kernel_path} not found")
        sys.exit(1)
    
    image = create_disk_image(output_image, stage1_path, stage2_path, kernel_path)
    
    format_xcfs(image)
    
    add_file_to_xcfs(image, "test.txt", "Hello from XCFS!\n")
    add_file_to_xcfs(image, "readme.txt", "XC-OS v1.0\nCustom File System\n")
    add_file_to_xcfs(image, "welcome.txt", "Welcome to XC-OS!\nType 'help' for commands.\n")
    
    with open(output_image, 'wb') as f:
        f.write(image)
    
    print(f"\n{'='*50}")
    print(f"Disk image created: {output_image}")
    print(f"Total size: {len(image)} bytes ({len(image) // 1024 // 1024} MB)")
    print(f"{'='*50}")

if __name__ == "__main__":
    main()