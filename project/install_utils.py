#!/usr/bin/env python3
import os
import struct
import sys

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

def read_xcfs_header(image):
    header_data = image[XCFS_START * SECTOR_SIZE:(XCFS_START + 1) * SECTOR_SIZE]
    magic, version, total_sectors, file_count = struct.unpack('<IIII', header_data[:16])
    return {
        'magic': magic,
        'version': version,
        'total_sectors': total_sectors,
        'file_count': file_count
    }

def read_xcfs_entries(image, count):
    entries = []
    entries_per_sector = SECTOR_SIZE // 512
    if entries_per_sector == 0:
        entries_per_sector = 1
    
    sectors_needed = (count + entries_per_sector - 1) // entries_per_sector
    
    for s in range(sectors_needed):
        sector_offset = (XCFS_START + 1 + s) * SECTOR_SIZE
        sector_data = image[sector_offset:sector_offset + SECTOR_SIZE]
        
        for e in range(entries_per_sector):
            if len(entries) >= count:
                break
            
            offset = e * 512
            entry_data = sector_data[offset:offset + 512]
            
            path = entry_data[:XCFS_MAX_PATH].decode('ascii', errors='ignore').rstrip('\x00')
            start_sector, size, parent_idx = struct.unpack('<III', entry_data[XCFS_MAX_PATH:XCFS_MAX_PATH + 12])
            entry_type, flags = struct.unpack('<BB', entry_data[XCFS_MAX_PATH + 12:XCFS_MAX_PATH + 14])
            
            entries.append({
                'path': path,
                'start_sector': start_sector,
                'size': size,
                'parent_idx': parent_idx,
                'type': entry_type,
                'flags': flags
            })
    
    return entries

def find_free_sector(entries):
    max_sector = XCFS_DATA_START
    for entry in entries:
        if entry['type'] == XCFS_TYPE_FILE and entry['size'] > 0:
            sectors = (entry['size'] + SECTOR_SIZE - 1) // SECTOR_SIZE
            end = entry['start_sector'] + sectors
            if end > max_sector:
                max_sector = end
    return max_sector

def install_binary(image_path, bin_path, dest_path):
    print(f"Installing {bin_path} to {dest_path}...")
    
    with open(image_path, 'r+b') as img:
        image_data = bytearray(img.read())
        
        header = read_xcfs_header(image_data)
        if header['magic'] != XCFS_MAGIC:
            print("ERROR: Invalid XCFS magic")
            return False
        
        entries = read_xcfs_entries(image_data, header['file_count'])
        
        for entry in entries:
            if entry['path'] == dest_path:
                print(f"  Removing existing {dest_path}")
                entries.remove(entry)
                header['file_count'] -= 1
                break
        
        with open(bin_path, 'rb') as f:
            bin_data = f.read()
        
        bin_size = len(bin_data)
        start_sector = find_free_sector(entries)
        
        new_entry = {
            'path': dest_path,
            'start_sector': start_sector,
            'size': bin_size,
            'parent_idx': 0,
            'type': XCFS_TYPE_FILE,
            'flags': XCFS_FLAG_EXECUTABLE
        }
        entries.append(new_entry)
        header['file_count'] += 1
        
        sectors_needed = (bin_size + SECTOR_SIZE - 1) // SECTOR_SIZE
        for s in range(sectors_needed):
            sector_offset = (start_sector + s) * SECTOR_SIZE
            chunk_start = s * SECTOR_SIZE
            chunk_end = min(chunk_start + SECTOR_SIZE, bin_size)
            chunk = bin_data[chunk_start:chunk_end]
            
            image_data[sector_offset:sector_offset + len(chunk)] = chunk
            if len(chunk) < SECTOR_SIZE:
                image_data[sector_offset + len(chunk):sector_offset + SECTOR_SIZE] = b'\x00' * (SECTOR_SIZE - len(chunk))
        
        header_bytes = struct.pack('<IIII', header['magic'], header['version'], 
                                   header['total_sectors'], header['file_count'])
        header_bytes += b'\x00' * (SECTOR_SIZE - len(header_bytes))
        image_data[XCFS_START * SECTOR_SIZE:(XCFS_START + 1) * SECTOR_SIZE] = header_bytes
        
        entries_per_sector = SECTOR_SIZE // 512
        if entries_per_sector == 0:
            entries_per_sector = 1
        
        for i, entry in enumerate(entries):
            sector_idx = i // entries_per_sector
            entry_idx = i % entries_per_sector
            sector_offset = (XCFS_START + 1 + sector_idx) * SECTOR_SIZE + entry_idx * 512
            
            path_bytes = entry['path'].encode('ascii')[:XCFS_MAX_PATH]
            path_bytes += b'\x00' * (XCFS_MAX_PATH - len(path_bytes))
            
            entry_bytes = path_bytes
            entry_bytes += struct.pack('<III', entry['start_sector'], entry['size'], entry['parent_idx'])
            entry_bytes += struct.pack('<BB', entry['type'], entry['flags'])
            entry_bytes += b'\x00' * (512 - len(entry_bytes))
            
            image_data[sector_offset:sector_offset + 512] = entry_bytes
        
        img.seek(0)
        img.write(image_data)
        
        print(f"  OK: {dest_path} ({bin_size} bytes, sector {start_sector})")
        return True

def main():
    if len(sys.argv) < 2:
        print("Usage: install_utils.py <image_path> [utils_dir]")
        sys.exit(1)
    
    image_path = sys.argv[1]
    utils_dir = sys.argv[2] if len(sys.argv) > 2 else "build/utils"
    
    if not os.path.exists(image_path):
        print(f"ERROR: Image {image_path} not found")
        sys.exit(1)
    
    if not os.path.exists(utils_dir):
        print(f"ERROR: Utils directory {utils_dir} not found")
        sys.exit(1)
    
    utils = []
    for filename in os.listdir(utils_dir):
        if filename.endswith('.bin'):
            utils.append(filename[:-4])
    
    print(f"Installing {len(utils)} utilities to {image_path}...")
    print()
    
    for util in utils:
        bin_path = os.path.join(utils_dir, f"{util}.bin")
        dest_path = f"/bin/{util}"
        install_binary(image_path, bin_path, dest_path)
    
    print()
    print(f"Installation complete: {len(utils)} utilities installed")

if __name__ == '__main__':
    main()
