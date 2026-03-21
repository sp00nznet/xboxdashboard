#!/usr/bin/env python3
"""
Extract Xbox Dashboard files from a QCOW2 HDD image with FATX filesystem.

Xbox HDD partition layout (standard 8GB):
  Partition 0: 0x80000 - 0x2EE80000 (UDATA/TDATA - E:)
  Partition 1: 0x2EE80000 - 0x5DC80000 (Cache - X:, Y:, Z:)
  Partition 2: 0x5DC80000 - 0x5FD80000 (System/Dashboard - C:)

But for small images (like this ~103MB one), offsets may differ.
We'll scan for FATX magic to find partitions.
"""

import struct
import os
import sys
import zlib

# ── QCOW2 Reader ──────────────────────────────────────────────

QCOW2_MAGIC = 0x514649FB  # "QFI\xfb"

def read_qcow2_to_raw(path):
    """Read a QCOW2 image and return the raw disk bytes."""
    with open(path, 'rb') as f:
        data = f.read()

    # Parse QCOW2 header
    magic = struct.unpack_from('>I', data, 0)[0]
    if magic != QCOW2_MAGIC:
        print(f"Not a QCOW2 file (magic: 0x{magic:08X})")
        sys.exit(1)

    version = struct.unpack_from('>I', data, 4)[0]
    print(f"QCOW2 version: {version}")

    backing_file_offset = struct.unpack_from('>Q', data, 8)[0]
    backing_file_size = struct.unpack_from('>I', data, 16)[0]
    cluster_bits = struct.unpack_from('>I', data, 20)[0]
    disk_size = struct.unpack_from('>Q', data, 24)[0]
    crypt_method = struct.unpack_from('>I', data, 32)[0]
    l1_size = struct.unpack_from('>I', data, 36)[0]
    l1_table_offset = struct.unpack_from('>Q', data, 40)[0]
    refcount_table_offset = struct.unpack_from('>Q', data, 48)[0]
    refcount_table_clusters = struct.unpack_from('>I', data, 56)[0]
    nb_snapshots = struct.unpack_from('>I', data, 60)[0]
    snapshots_offset = struct.unpack_from('>Q', data, 64)[0]

    cluster_size = 1 << cluster_bits
    l2_entries = cluster_size // 8  # Each L2 entry is 8 bytes

    print(f"Disk size: {disk_size} bytes ({disk_size // (1024*1024)} MB)")
    print(f"Cluster size: {cluster_size} bytes")
    print(f"L1 table entries: {l1_size}")
    print(f"L1 table offset: 0x{l1_table_offset:X}")

    # Read L1 table
    l1_table = []
    for i in range(l1_size):
        off = l1_table_offset + i * 8
        entry = struct.unpack_from('>Q', data, off)[0]
        l1_table.append(entry)

    # Build raw disk image
    raw = bytearray(disk_size)

    for l1_idx, l1_entry in enumerate(l1_table):
        l2_offset = l1_entry & 0x00FFFFFFFFFFFE00  # Mask off flags
        if l2_offset == 0:
            continue  # Unallocated L1 entry

        # Read L2 table
        for l2_idx in range(l2_entries):
            l2_entry_off = l2_offset + l2_idx * 8
            if l2_entry_off + 8 > len(data):
                continue
            l2_entry = struct.unpack_from('>Q', data, l2_entry_off)[0]

            compressed = (l2_entry >> 62) & 1
            cluster_offset = l2_entry & 0x00FFFFFFFFFFFE00

            if cluster_offset == 0 and not compressed:
                continue  # Unallocated cluster

            disk_offset = (l1_idx * l2_entries + l2_idx) * cluster_size
            if disk_offset >= disk_size:
                continue

            if compressed:
                # Compressed cluster
                # For compressed clusters, the offset and size are packed differently
                nb_sectors = ((l2_entry >> 62) & 0) # compressed bit already checked
                # compressed cluster: bits 61-0 contain host offset, we need to figure out size
                # Actually for QCOW2, compressed entries encode offset in lower bits
                # and the number of 512-byte sectors in upper bits (after the compressed flag)
                # Let's try to handle this
                host_offset = l2_entry & ((1 << 62) - 1)  # lower 62 bits
                # The size is determined by the next cluster's offset - this one
                # For simplicity, try reading from the offset and decompressing
                # The compressed size is stored as: bits [62-cluster_bits] = offset,
                # bits [cluster_bits-1:0] = nb_additional_sectors
                sector_bits = cluster_bits - 9  # number of bits for additional sectors
                compressed_offset = (l2_entry & ((1 << 62) - 1)) >> sector_bits
                nb_csectors = (l2_entry & ((1 << sector_bits) - 1)) + 1
                compressed_size = nb_csectors * 512

                if compressed_offset + compressed_size <= len(data):
                    try:
                        decompressed = zlib.decompress(
                            data[compressed_offset:compressed_offset + compressed_size],
                            -15  # raw deflate
                        )
                        copy_size = min(len(decompressed), cluster_size, disk_size - disk_offset)
                        raw[disk_offset:disk_offset + copy_size] = decompressed[:copy_size]
                    except:
                        # Try with zlib header
                        try:
                            decompressed = zlib.decompress(
                                data[compressed_offset:compressed_offset + compressed_size]
                            )
                            copy_size = min(len(decompressed), cluster_size, disk_size - disk_offset)
                            raw[disk_offset:disk_offset + copy_size] = decompressed[:copy_size]
                        except:
                            pass
            else:
                # Standard cluster
                if cluster_offset + cluster_size <= len(data):
                    copy_size = min(cluster_size, disk_size - disk_offset)
                    raw[disk_offset:disk_offset + copy_size] = data[cluster_offset:cluster_offset + copy_size]

    return bytes(raw)


# ── FATX Reader ────────────────────────────────────────────────

FATX_MAGIC = b'FATX'

def find_fatx_partitions(raw_disk):
    """Scan raw disk for FATX partition magic bytes."""
    partitions = []

    # Known Xbox HDD partition offsets for standard layout
    known_offsets = [
        0x00080000,     # Partition 0 (E: UDATA/TDATA)
        0x2EE80000,     # Partition 1 (Cache)
        0x5DC80000,     # Partition 2 (C: System/Dashboard)
    ]

    # Also scan for FATX magic at 4KB boundaries
    print("Scanning for FATX partitions...")
    for offset in range(0, min(len(raw_disk), 0x80000000), 0x1000):
        if raw_disk[offset:offset+4] == FATX_MAGIC:
            partitions.append(offset)
            print(f"  Found FATX at offset 0x{offset:08X}")

    return partitions


def read_fatx_partition(raw_disk, partition_offset, max_size=None):
    """Read files from a FATX partition."""
    if raw_disk[partition_offset:partition_offset+4] != FATX_MAGIC:
        print(f"No FATX magic at 0x{partition_offset:08X}")
        return []

    # FATX superblock
    magic = raw_disk[partition_offset:partition_offset+4]
    volume_id = struct.unpack_from('<I', raw_disk, partition_offset + 4)[0]
    cluster_size_sectors = struct.unpack_from('<I', raw_disk, partition_offset + 8)[0]
    num_fat_copies = struct.unpack_from('<H', raw_disk, partition_offset + 12)[0]

    # Cluster size in bytes (sectors are 512 bytes)
    if cluster_size_sectors == 0:
        cluster_size_sectors = 32  # Default: 16KB clusters
    cluster_size = cluster_size_sectors * 512

    print(f"\nFATX partition at 0x{partition_offset:08X}:")
    print(f"  Volume ID: 0x{volume_id:08X}")
    print(f"  Cluster size: {cluster_size} bytes ({cluster_size_sectors} sectors)")
    print(f"  FAT copies: {num_fat_copies}")

    # Determine partition size (use max_size or estimate)
    if max_size is None:
        max_size = len(raw_disk) - partition_offset

    # Calculate number of clusters
    # FAT starts at offset 0x1000 (4096) from partition start
    fat_offset = partition_offset + 0x1000

    # Determine if 16-bit or 32-bit FAT entries
    # Xbox uses 16-bit FAT for partitions < 256MB, 32-bit for larger
    partition_size = min(max_size, len(raw_disk) - partition_offset)
    num_clusters = partition_size // cluster_size

    if num_clusters < 0xFFF0:
        fat_entry_size = 2  # 16-bit FAT
        fat_eof = 0xFFF8
        fat_free = 0x0000
    else:
        fat_entry_size = 4  # 32-bit FAT
        fat_eof = 0xFFFFFFF8
        fat_free = 0x00000000

    print(f"  FAT entry size: {fat_entry_size * 8}-bit")
    print(f"  Estimated clusters: {num_clusters}")

    # FAT table size
    fat_size = num_clusters * fat_entry_size
    # Round up to cluster boundary
    fat_size_aligned = ((fat_size + cluster_size - 1) // cluster_size) * cluster_size

    # Data region starts after FAT
    data_offset = fat_offset + fat_size_aligned

    print(f"  FAT offset: 0x{fat_offset:08X}")
    print(f"  FAT size: 0x{fat_size_aligned:X}")
    print(f"  Data offset: 0x{data_offset:08X}")

    def read_fat_entry(cluster):
        off = fat_offset + cluster * fat_entry_size
        if off + fat_entry_size > len(raw_disk):
            return fat_eof
        if fat_entry_size == 2:
            return struct.unpack_from('<H', raw_disk, off)[0]
        else:
            return struct.unpack_from('<I', raw_disk, off)[0]

    def cluster_to_offset(cluster):
        # Cluster 1 is the first data cluster
        return data_offset + (cluster - 1) * cluster_size

    def read_cluster_chain(start_cluster):
        """Read all data from a cluster chain."""
        chain_data = bytearray()
        cluster = start_cluster
        visited = set()
        while cluster >= 1 and cluster < fat_eof and cluster not in visited:
            visited.add(cluster)
            off = cluster_to_offset(cluster)
            if off + cluster_size > len(raw_disk):
                break
            chain_data.extend(raw_disk[off:off + cluster_size])
            cluster = read_fat_entry(cluster)
        return bytes(chain_data)

    def read_directory(start_cluster, path=""):
        """Recursively read a FATX directory."""
        files = []

        if start_cluster == 0:
            # Root directory is at cluster 1
            start_cluster = 1

        dir_data = read_cluster_chain(start_cluster)

        # Each directory entry is 64 bytes
        entry_size = 64
        for i in range(0, len(dir_data), entry_size):
            entry = dir_data[i:i + entry_size]
            if len(entry) < entry_size:
                break

            name_length = entry[0]
            if name_length == 0x00 or name_length == 0xFF:
                continue  # Empty or deleted entry
            if name_length == 0xE5:
                continue  # Deleted

            # Valid name lengths are 1-42
            if name_length > 42:
                continue

            attributes = entry[1]
            name = entry[2:2 + name_length].decode('ascii', errors='replace').rstrip('\x00')

            first_cluster = struct.unpack_from('<I', entry, 44)[0]
            file_size = struct.unpack_from('<I', entry, 48)[0]

            # Timestamps at offsets 52, 56, 60 (creation, write, access)

            is_directory = (attributes & 0x10) != 0
            full_path = f"{path}/{name}" if path else name

            if is_directory:
                print(f"  DIR:  {full_path}/")
                if first_cluster >= 1 and first_cluster < num_clusters:
                    sub_files = read_directory(first_cluster, full_path)
                    files.extend(sub_files)
            else:
                print(f"  FILE: {full_path} ({file_size} bytes)")
                file_data = None
                if first_cluster >= 1 and first_cluster < num_clusters and file_size > 0:
                    file_data = read_cluster_chain(first_cluster)[:file_size]
                files.append({
                    'path': full_path,
                    'size': file_size,
                    'data': file_data,
                    'attributes': attributes,
                })

        return files

    print("\nReading directory tree...")
    files = read_directory(0)
    return files


def extract_files(files, output_dir):
    """Write extracted files to disk."""
    os.makedirs(output_dir, exist_ok=True)

    for f in files:
        if f['data'] is None:
            continue

        out_path = os.path.join(output_dir, f['path'].replace('/', os.sep))
        os.makedirs(os.path.dirname(out_path), exist_ok=True)

        with open(out_path, 'wb') as fh:
            fh.write(f['data'])
        print(f"  Extracted: {out_path} ({f['size']} bytes)")


# ── Main ───────────────────────────────────────────────────────

def main():
    qcow2_path = sys.argv[1] if len(sys.argv) > 1 else "extracted/Firmware/xbox_hdd.qcow2"
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "game"

    print(f"Reading QCOW2: {qcow2_path}")
    raw_disk = read_qcow2_to_raw(qcow2_path)
    print(f"Raw disk size: {len(raw_disk)} bytes ({len(raw_disk) // (1024*1024)} MB)")

    # Save raw image for debugging
    raw_path = "extracted/xbox_hdd.raw"
    with open(raw_path, 'wb') as f:
        f.write(raw_disk)
    print(f"Saved raw image: {raw_path}")

    # Find and extract FATX partitions
    partitions = find_fatx_partitions(raw_disk)

    if not partitions:
        print("\nNo FATX partitions found! Trying to scan the raw image...")
        # Dump first few KB for analysis
        print("First 64 bytes of disk:")
        for i in range(0, 64, 16):
            hex_str = ' '.join(f'{b:02X}' for b in raw_disk[i:i+16])
            ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in raw_disk[i:i+16])
            print(f"  {i:04X}: {hex_str}  {ascii_str}")
        return

    # Extract each partition
    for i, part_offset in enumerate(partitions):
        # Determine partition end (next partition start or end of disk)
        if i + 1 < len(partitions):
            part_size = partitions[i + 1] - part_offset
        else:
            part_size = len(raw_disk) - part_offset

        print(f"\n{'='*60}")
        print(f"Extracting partition {i} at offset 0x{part_offset:08X} (size: {part_size // 1024}KB)")
        print(f"{'='*60}")

        part_dir = f"{output_dir}/partition_{i}" if len(partitions) > 1 else output_dir
        files = read_fatx_partition(raw_disk, part_offset, part_size)

        if files:
            print(f"\nExtracting {len(files)} files to {part_dir}/...")
            extract_files(files, part_dir)
        else:
            print("No files found in this partition.")

    print(f"\nDone! Dashboard files should be in {output_dir}/")


if __name__ == '__main__':
    main()
