# -*- coding: utf-8 -*-
#!/usr/bin/env python3
# gen_magic_header.py - Generate a .xbin firmware package for IAP Bootloader
# Header layout matches the device-side magic_header_t exactly.

import argparse
import os
import struct
import sys
import zlib

MAGIC_HEADER_MAGIC    = 0x4D414749  # 'MAGI'
DATA_TYPE_APP         = 0           # MAGIC_HEADER_TYPE_APP
MAGIC_HEADER_SIZE     = 256
MAGIC_HEADER_CRC_SIZE = 252         # CRC covers bytes 0..251

DEFAULT_HEADER_ADDR = 0x0800C000
DEFAULT_APP_ADDR    = 0x08010000

def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF

def build_magic_header(app_bin: bytes, version: str, header_addr: int, app_addr: int) -> bytes:
    data_length = len(app_bin)
    data_crc32  = crc32(app_bin)
    data_offset = MAGIC_HEADER_SIZE  # app binary starts right after the header

    ver_bytes = version.encode("ascii", errors="ignore")[:127]
    ver_field = ver_bytes + b"\x00" * (128 - len(ver_bytes))

    reserved1 = [0] * 6
    reserved2 = [0] * 11
    reserved3 = [0] * 6
    bitmask   = 0

    prefix = (
        struct.pack("<I", MAGIC_HEADER_MAGIC)   # magic @0
        + struct.pack("<I", bitmask)            # bitmask @4
        + struct.pack("<6I", *reserved1)        # reserved1[6] @8
        + struct.pack("<I", DATA_TYPE_APP)      # data_type @32
        + struct.pack("<I", data_offset)        # data_offset @36
        + struct.pack("<I", app_addr)           # data_address @40
        + struct.pack("<I", data_length)        # data_length @44
        + struct.pack("<I", data_crc32)         # data_crc32 @48
        + struct.pack("<11I", *reserved2)       # reserved2[11] @52
        + ver_field                             # version[128] @96
        + struct.pack("<6I", *reserved3)        # reserved3[6] @224
        + struct.pack("<I", header_addr)        # this_address @248
    )

    assert len(prefix) == MAGIC_HEADER_CRC_SIZE, f"prefix len {len(prefix)} != {MAGIC_HEADER_CRC_SIZE}"
    this_crc32 = crc32(prefix)
    header = prefix + struct.pack("<I", this_crc32)  # this_crc32 @252

    assert len(header) == MAGIC_HEADER_SIZE, f"header len {len(header)} != {MAGIC_HEADER_SIZE}"
    return header

def main() -> None:
    parser = argparse.ArgumentParser(
        prog="gen_magic_header.py",
        description="Generate a .xbin firmware package for IAP Bootloader (header matches device layout)",
    )
    parser.add_argument("input", metavar="INPUT.bin",
                        help="Input raw application binary (.bin)")
    parser.add_argument("--version", default="v1.0.0",
                        help="Firmware version string (default: v1.0.0)")
    parser.add_argument("--app-address", metavar="ADDR", type=lambda x: int(x, 0),
                        default=DEFAULT_APP_ADDR,
                        help=f"Flash address where the app binary is programmed (default: 0x{DEFAULT_APP_ADDR:08X})")
    parser.add_argument("--header-address", metavar="ADDR", type=lambda x: int(x, 0),
                        default=DEFAULT_HEADER_ADDR,
                        help=f"Flash address where the magic header is programmed (default: 0x{DEFAULT_HEADER_ADDR:08X})")
    parser.add_argument("-o", "--output", metavar="OUTPUT.xbin",
                        help="Output file path (default: INPUT with .xbin extension)")
    parser.add_argument("axf", metavar="INPUT.axf", nargs="?", default=None,
                        help="Optional ELF/AXF file (ignored, for Keil compatibility)")
    args = parser.parse_args()

    if not os.path.isfile(args.input):
        print(f"ERROR: input file not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    with open(args.input, "rb") as fh:
        app_bin = fh.read()
    if not app_bin:
        print("ERROR: input file is empty", file=sys.stderr)
        sys.exit(1)

    output_path = args.output or (os.path.splitext(args.input)[0] + ".xbin")

    header = build_magic_header(app_bin, args.version, args.header_address, args.app_address)
    xbin = header + app_bin

    with open(output_path, "wb") as fh:
        fh.write(xbin)

    print(f"Output        : {output_path}  ({len(xbin)} bytes)")
    print(f"Header addr   : 0x{args.header_address:08X}")
    print(f"Header CRC32  : 0x{crc32(header[:MAGIC_HEADER_CRC_SIZE]):08X}")
    print(f"App addr      : 0x{args.app_address:08X}")
    print(f"App size      : {len(app_bin)} bytes ({len(app_bin)/1024:.2f} KB)")
    print(f"App CRC32     : 0x{crc32(app_bin):08X}")
    print(f"Version       : {args.version}")

if __name__ == "__main__":
    main()
