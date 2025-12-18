#! /usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import time
import zlib

# typedef struct
# {
#     uint32_t magic;         // 魔数，用于标识这是一个有效的魔术头
#     uint32_t bitmask;       // 位掩码，用于标识哪些字段有效
#     uint32_t reserved1[6];  // 保留字段，供将来扩展使用

#     uint32_t data_type;     // 类型，根据type类型选择固件下载位置
#     uint32_t data_offset;   // 固件文件相对于magic header的偏移
#     uint32_t data_address;  // 固件写入的实际地址
#     uint32_t data_length;   // 固件长度
#     uint32_t data_crc32;    // 固件的CRC32校验值
#     uint32_t reserved2[11]; // 保留字段，供将来扩展使用

#     char version[128];      // 固件版本字符串

#     uint32_t reserved3[6];  // 保留字段，供将来扩展使用
#     uint32_t this_address;  // 该结构体在存储介质中的实际地址
#     uint32_t this_crc32;    // 该结构体本身的CRC32校验值
# } magic_header_t;

MAGIC_HEADER_MAGIC = 0x4D414749  # 'MAGI' in ASCII
DATA_TYPE_FIRMWARE = 1
BL_VERSION_MAJOR = 1
BL_VERSION_MINOR = 0
BL_VERSION_PATCH = 0
BL_VERSION_EXTRA = "alpha"

def main():
    project_root = os.path.realpath(os.path.join(os.path.dirname(__file__), ".."))

    if len(sys.argv) < 2:
        print("Usage: gen_magic_header.py <input_file>")
        sys.exit(1)

    binfile = os.path.realpath(sys.argv[1])
    if not os.path.isfile(binfile):
        print(f"Error: File '{binfile}' does not exist.")
        sys.exit(1)

    with open(binfile, "rb") as f:
        bin_data = f.read()
    bin_crc = zlib.crc32(bin_data) & 0xFFFFFFFF  # 计算CRC32校验值

    # 构建magic header
    header = []

    header.append((MAGIC_HEADER_MAGIC).to_bytes(4, byteorder='little'))  # magic
    header.append((0).to_bytes(4, byteorder='little'))  # bitmask
    header.append((0).to_bytes(4 * 6, byteorder='little'))  # reserved1

    header.append((DATA_TYPE_FIRMWARE).to_bytes(4, byteorder='little'))  # data_type
    header.append((4096).to_bytes(4, byteorder='little'))  # data_offset
    header.append((0x08010000).to_bytes(4, byteorder='little'))  # data_address
    header.append((len(bin_data)).to_bytes(4, byteorder='little'))  # data_length
    header.append((bin_crc).to_bytes(4, byteorder='little'))  # data_crc32
    header.append((0).to_bytes(4 * 11, byteorder='little'))  # reserved2

    version_date = time.strftime("%y%m%d", time.localtime())
    version_time = time.strftime("%H%M", time.localtime())
    version_str = f"v{BL_VERSION_MAJOR}.{BL_VERSION_MINOR}.{BL_VERSION_PATCH}-{version_date}-{version_time}-{BL_VERSION_EXTRA}"
    version_bytes = version_str.encode('ascii')
    version_bytes = version_bytes.ljust(128, b'\x00')
    header.append(version_bytes)  # version

    header.append((0).to_bytes(4 * 6, byteorder='little'))  # reserved3
    header.append((0x0800C000).to_bytes(4, byteorder='little'))  # this_address

    this_crc = zlib.crc32(b''.join(header)) & 0xFFFFFFFF  # 计算当前结构体的CRC32校验值
    header.append((this_crc).to_bytes(4, byteorder='little'))  # this_crc32

    magic_header = b''.join(header)
    magic_header = magic_header.ljust(4096, b'\x00')  # 填充到 4096 字节

    # 生成带有魔术头的二进制文件
    upgrade_filename = os.path.splitext(os.path.basename(binfile))[0] + "_upgrade.xbin"
    output_path = os.path.join(project_root, "build", upgrade_filename)
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "wb") as f:
        f.write(b''.join([magic_header, bin_data]))# Write magic header and binary data to output file
    print(f"Generated upgrade file: {output_path}")


if __name__ == "__main__":
    main()
