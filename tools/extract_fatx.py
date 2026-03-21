#!/usr/bin/env python3
"""Extract files from Xbox FATX partition in a raw disk image."""

import struct
import os
import sys

FATX_MAGIC = b'FATX'

def extract_fatx(disk_path, part_offset, output_dir, part_size_est=500*1024*1024):
    with open(disk_path, 'rb') as f:
        f.seek(part_offset)
        header = f.read(0x1000)

        if header[0:4] != FATX_MAGIC:
            print(f"No FATX at 0x{part_offset:08X}")
            return

        cluster_sectors = struct.unpack_from('<I', header, 8)[0]
        cluster_size = cluster_sectors * 512

        num_clusters = part_size_est // cluster_size
        fat_entry_size = 2 if num_clusters < 0xFFF0 else 4
        fat_eof = 0xFFF8 if fat_entry_size == 2 else 0xFFFFFFF8

        fat_off = part_offset + 0x1000
        fat_size = num_clusters * fat_entry_size
        fat_size_aligned = ((fat_size + cluster_size - 1) // cluster_size) * cluster_size
        data_off = fat_off + fat_size_aligned

        # Read entire FAT
        f.seek(fat_off)
        fat_data = f.read(num_clusters * fat_entry_size)

        def read_fat(cluster):
            off = cluster * fat_entry_size
            if off + fat_entry_size > len(fat_data):
                return fat_eof
            if fat_entry_size == 2:
                return struct.unpack_from('<H', fat_data, off)[0]
            return struct.unpack_from('<I', fat_data, off)[0]

        def cluster_offset(cluster):
            return data_off + (cluster - 1) * cluster_size

        def read_chain(start_cluster):
            result = bytearray()
            c = start_cluster
            visited = set()
            while c >= 1 and c < fat_eof and c not in visited:
                visited.add(c)
                off = cluster_offset(c)
                f.seek(off)
                result.extend(f.read(cluster_size))
                c = read_fat(c)
            return bytes(result)

        def read_dir(start_cluster, path=""):
            if start_cluster == 0:
                start_cluster = 1
            dir_data = read_chain(start_cluster)

            for i in range(0, len(dir_data), 64):
                entry = dir_data[i:i+64]
                if len(entry) < 64:
                    break
                name_len = entry[0]
                if name_len == 0 or name_len == 0xFF or name_len == 0xE5:
                    continue
                if name_len > 42:
                    continue

                attr = entry[1]
                name = entry[2:2+name_len].decode('ascii', errors='replace').rstrip('\x00')
                first_cluster = struct.unpack_from('<I', entry, 44)[0]
                file_size = struct.unpack_from('<I', entry, 48)[0]
                full_path = f"{path}/{name}" if path else name

                if attr & 0x10:  # Directory
                    print(f"  DIR:  {full_path}/")
                    if first_cluster >= 1:
                        read_dir(first_cluster, full_path)
                else:
                    print(f"  FILE: {full_path} ({file_size:,} bytes)")
                    if first_cluster >= 1 and file_size > 0:
                        file_data = read_chain(first_cluster)[:file_size]
                        out_path = os.path.join(output_dir, full_path.replace('/', os.sep))
                        os.makedirs(os.path.dirname(out_path) if os.path.dirname(out_path) else output_dir, exist_ok=True)
                        with open(out_path, 'wb') as out_f:
                            out_f.write(file_data)

        print(f"Extracting FATX at 0x{part_offset:08X} to {output_dir}/")
        os.makedirs(output_dir, exist_ok=True)
        read_dir(0)
        print("Done!")


if __name__ == '__main__':
    disk_path = sys.argv[1] if len(sys.argv) > 1 else "extracted/xbox_hdd.raw"
    part_offset = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x8CA80000
    output_dir = sys.argv[3] if len(sys.argv) > 3 else "game"
    part_size = int(sys.argv[4]) if len(sys.argv) > 4 else 500*1024*1024

    extract_fatx(disk_path, part_offset, output_dir, part_size)
