#!/usr/bin/env python3
import sys
import struct

SECTOR_SIZE = 512
XCFS_MAGIC = 0x58434653
XCFS_VERSION = 2
XCFS_START = 512
XCFS_DATA_START = 2048
XCFS_MAX_PATH = 248

XCFS_TYPE_FILE = 0x01
XCFS_TYPE_DIR = 0x02
XCFS_FLAG_PROTECTED = 0x04
XCFS_FLAG_EXECUTABLE = 0x08

def format_xcfs(image_path):
    print(f"Formatting XCFS v{XCFS_VERSION} in {image_path}...")
    
    with open(image_path, 'r+b') as f:
        f.seek(0, 2)
        total_size = f.tell()
        total_sectors = total_size // SECTOR_SIZE
        
        header = struct.pack('<IIII', XCFS_MAGIC, XCFS_VERSION, total_sectors, 0)
        header += b'\x00' * (SECTOR_SIZE - len(header))
        
        f.seek(XCFS_START * SECTOR_SIZE)
        f.write(header)
        
        for i in range(XCFS_START + 1, XCFS_DATA_START):
            f.seek(i * SECTOR_SIZE)
            f.write(b'\x00' * SECTOR_SIZE)
        
        print(f"  XCFS header at sector {XCFS_START}")
        print(f"  XCFS entries: sectors {XCFS_START + 1}-{XCFS_DATA_START - 1}")
        print(f"  XCFS data area: sector {XCFS_DATA_START}+")
        print(f"  Total sectors: {total_sectors}")

def add_directory(image_path, dirpath):
    print(f"Creating directory: {dirpath}")
    
    with open(image_path, 'r+b') as f:
        f.seek(XCFS_START * SECTOR_SIZE)
        header = f.read(16)
        magic, version, total_sectors, file_count = struct.unpack('<IIII', header)
        
        if magic != XCFS_MAGIC:
            print(f"ERROR: Invalid XCFS magic: 0x{magic:08X}")
            return False
        
        if file_count >= 256:
            print("ERROR: Maximum file count reached")
            return False
        
        dirpath_bytes = dirpath.encode('ascii')
        if len(dirpath_bytes) > XCFS_MAX_PATH:
            dirpath_bytes = dirpath_bytes[:XCFS_MAX_PATH]
        
        entry = bytearray(512)
        entry[0:len(dirpath_bytes)] = dirpath_bytes
        entry[XCFS_MAX_PATH:XCFS_MAX_PATH + 4] = struct.pack('<I', 0)
        entry[XCFS_MAX_PATH + 4:XCFS_MAX_PATH + 8] = struct.pack('<I', 0)
        entry[XCFS_MAX_PATH + 8:XCFS_MAX_PATH + 12] = struct.pack('<I', 0)
        entry[XCFS_MAX_PATH + 12] = XCFS_TYPE_DIR
        entry[XCFS_MAX_PATH + 13] = 0
        
        entries_per_sector = SECTOR_SIZE // 512
        if entries_per_sector == 0:
            entries_per_sector = 1
        
        sector_idx = file_count // entries_per_sector
        entry_idx = file_count % entries_per_sector
        entry_offset = (XCFS_START + 1 + sector_idx) * SECTOR_SIZE + entry_idx * 512
        
        f.seek(entry_offset)
        f.write(entry)
        
        file_count += 1
        f.seek(XCFS_START * SECTOR_SIZE + 12)
        f.write(struct.pack('<I', file_count))
        
        print(f"  Created: {dirpath}")
        return True

def main():
    if len(sys.argv) < 2:
        print("Usage: format_xcfs.py <image_path>")
        sys.exit(1)
    
    image_path = sys.argv[1]
    
    format_xcfs(image_path)
    
    add_directory(image_path, "/bin")
    add_directory(image_path, "/etc")
    add_directory(image_path, "/home")
    add_directory(image_path, "/tmp")
    
    print("\nXCFS formatting complete!")

if __name__ == '__main__':
    main()

# Fuck this shit.
