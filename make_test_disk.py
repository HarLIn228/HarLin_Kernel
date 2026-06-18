import struct
import sys

def make_fat32_disk(path, size_mb=20):
    sector_size = 512
    total_sectors = (size_mb * 1024 * 1024) // sector_size
    partition_start = 2048
    partition_sectors = total_sectors - partition_start

    bytes_per_sector = 512
    sectors_per_cluster = 1
    reserved_sectors = 32
    number_of_fats = 2
    sectors_per_fat = 256
    root_cluster = 2
    total_clusters = (partition_sectors - reserved_sectors - number_of_fats * sectors_per_fat) // sectors_per_cluster

    first_data_sector = reserved_sectors + number_of_fats * sectors_per_fat
    data_clusters = partition_sectors - first_data_sector

    if data_clusters <= 4:
        print("Disk too small")
        sys.exit(1)

    img = bytearray(total_sectors * sector_size)

    # MBR
    mbr = bytearray(sector_size)
    mbr[0x1BE + 0] = 0x80  # active
    mbr[0x1BE + 4] = 0x0C  # FAT32 LBA
    struct.pack_into('<I', mbr, 0x1BE + 8, partition_start)
    struct.pack_into('<I', mbr, 0x1BE + 12, partition_sectors)
    mbr[510] = 0x55
    mbr[511] = 0xAA
    img[0:sector_size] = mbr

    # FAT32 Boot Sector (BPB)
    bs = bytearray(sector_size)
    bs[0x00:0x03] = b'\xEB\x58\x90'
    bs[0x03:0x0B] = b'MSDOS5.0'
    struct.pack_into('<H', bs, 0x0B, bytes_per_sector)
    bs[0x0D] = sectors_per_cluster
    struct.pack_into('<H', bs, 0x0E, reserved_sectors)
    bs[0x10] = number_of_fats
    struct.pack_into('<H', bs, 0x11, 0)
    struct.pack_into('<H', bs, 0x13, 0)
    bs[0x15] = 0xF8
    struct.pack_into('<H', bs, 0x16, 0)
    struct.pack_into('<H', bs, 0x18, 63)
    struct.pack_into('<H', bs, 0x1A, 255)
    struct.pack_into('<I', bs, 0x1C, 0)
    struct.pack_into('<I', bs, 0x20, partition_sectors)
    struct.pack_into('<I', bs, 0x24, sectors_per_fat)
    struct.pack_into('<H', bs, 0x28, 0)
    struct.pack_into('<H', bs, 0x2A, 0)
    struct.pack_into('<I', bs, 0x2C, root_cluster)
    struct.pack_into('<H', bs, 0x30, 1)
    struct.pack_into('<H', bs, 0x32, 6)
    bs[0x40] = 0x80
    bs[0x41] = 0
    bs[0x42] = 0x29
    struct.pack_into('<I', bs, 0x43, 0x12345678)
    bs[0x47:0x52] = b'HARLIN     '
    bs[0x52:0x5A] = b'FAT32   '
    bs[510] = 0x55
    bs[511] = 0xAA

    poff = partition_start * sector_size
    img[poff:poff + sector_size] = bs

    # FSInfo
    fsinfo = bytearray(sector_size)
    struct.pack_into('<I', fsinfo, 0, 0x41615252)
    struct.pack_into('<I', fsinfo, 484, 0x61417272)
    struct.pack_into('<I', fsinfo, 488, 0xFFFFFFFF)
    struct.pack_into('<I', fsinfo, 492, 0xFFFFFFFF)
    struct.pack_into('<I', fsinfo, 508, 0xAA550000)
    img[poff + sector_size:poff + 2 * sector_size] = fsinfo

    # FAT tables
    fat = bytearray(sectors_per_fat * sector_size)
    struct.pack_into('<I', fat, 0, 0x0FFFFFF8)
    struct.pack_into('<I', fat, 4, 0x0FFFFFFF)
    struct.pack_into('<I', fat, 8, 0x0FFFFFFF)  # root dir cluster 2
    struct.pack_into('<I', fat, 12, 0x0FFFFFFF) # TEST.TXT cluster 3
    for i in range(number_of_fats):
        off = poff + (reserved_sectors + i * sectors_per_fat) * sector_size
        img[off:off + len(fat)] = fat

    # Root directory cluster (cluster 2)
    root_dir_sector = first_data_sector
    root_dir_off = poff + root_dir_sector * sector_size
    entry = bytearray(32)
    name = b'TEST    TXT'
    entry[0:11] = name
    entry[11] = 0x20
    struct.pack_into('<H', entry, 0x14, 0)
    struct.pack_into('<H', entry, 0x1A, 3)
    struct.pack_into('<I', entry, 0x1C, 18)
    img[root_dir_off:root_dir_off + 32] = entry

    # File data cluster (cluster 3)
    file_data = b'Hello from HarLin!'
    data_sector = first_data_sector + 1
    data_off = poff + data_sector * sector_size
    img[data_off:data_off + len(file_data)] = file_data

    with open(path, 'wb') as f:
        f.write(img)

    print(f"Created {path}: {size_mb}MB, partition at {partition_start}, FAT32 {total_clusters} clusters")

if __name__ == '__main__':
    make_fat32_disk('testdisk.img')
