#!/usr/bin/env python3
import argparse
import gzip
import os
import stat
import struct
import sys
import time
from pathlib import Path


PAGE_SIZE = 4096
BOOT_MAGIC = b"ANDROID!"


def align(value):
    return (value + PAGE_SIZE - 1) // PAGE_SIZE * PAGE_SIZE


def add_entry(entries, path, mode, data=b"", target=b"", rdev=(0, 0)):
    entries.append((path, mode, data, target, rdev))


def build_newc(root):
    entries = []
    now = int(time.time())
    ino = 300000

    def rel(path):
        value = path.relative_to(root).as_posix()
        return "." if value == "." else value

    for path in sorted(root.rglob("*")):
        st = os.lstat(path)
        name = rel(path)
        if stat.S_ISDIR(st.st_mode):
            add_entry(entries, name, stat.S_IFDIR | (st.st_mode & 0o777))
        elif stat.S_ISLNK(st.st_mode):
            add_entry(
                entries,
                name,
                stat.S_IFLNK | 0o777,
                target=os.readlink(path).encode(),
            )
        elif stat.S_ISREG(st.st_mode):
            add_entry(
                entries,
                name,
                stat.S_IFREG | (st.st_mode & 0o777),
                data=path.read_bytes(),
            )

    for dirname in (
        "dev",
        "dev/block",
        "dev/block/by-name",
        "dev/pts",
        "proc",
        "sys",
        "run",
        "tmp",
        "mnt",
        "mnt/arch",
        "mnt/data",
        "mnt/ubuntu",
    ):
        if not any(entry[0] == dirname for entry in entries):
            add_entry(entries, dirname, stat.S_IFDIR | 0o755)

    for name, perm, major, minor in (
        ("dev/console", 0o600, 5, 1),
        ("dev/null", 0o666, 1, 3),
        ("dev/zero", 0o666, 1, 5),
        ("dev/full", 0o666, 1, 7),
        ("dev/random", 0o666, 1, 8),
        ("dev/urandom", 0o666, 1, 9),
        ("dev/kmsg", 0o600, 1, 11),
        ("dev/tty", 0o666, 5, 0),
        ("dev/ptmx", 0o666, 5, 2),
    ):
        add_entry(entries, name, stat.S_IFCHR | perm, rdev=(major, minor))

    def pad4(buffer):
        while len(buffer) % 4:
            buffer.append(0)

    def write_entry(buffer, name, entry_ino, mode, data, target, rdev):
        body = target if stat.S_ISLNK(mode) else data
        namesize = len(name.encode()) + 1
        fields = [
            entry_ino,
            mode,
            0,
            0,
            1,
            now,
            len(body),
            0,
            0,
            rdev[0],
            rdev[1],
            namesize,
            0,
        ]
        buffer.extend(b"070701")
        for field in fields:
            buffer.extend(f"{field:08x}".encode())
        buffer.extend(name.encode() + b"\0")
        pad4(buffer)
        buffer.extend(body)
        pad4(buffer)

    output = bytearray()
    for ino, entry in enumerate(sorted(entries, key=lambda item: item[0]), ino + 1):
        write_entry(output, entry[0], ino, entry[1], entry[2], entry[3], entry[4])
    write_entry(output, "TRAILER!!!", ino + 1, 0, b"", b"", (0, 0))
    return bytes(output)


def gzip_ramdisk(raw):
    output = bytearray()
    with gzip.GzipFile(filename="", mode="wb", fileobj=BytesWriter(output), mtime=0) as gz:
        gz.write(raw)
    return bytes(output)


class BytesWriter:
    def __init__(self, output):
        self.output = output

    def write(self, data):
        self.output.extend(data)
        return len(data)

    def flush(self):
        return None


def pack_boot_image(base_image, ramdisk, out_path):
    base = base_image.read_bytes()
    if base[:8] != BOOT_MAGIC:
        raise ValueError(f"{base_image} is not an Android boot image")

    kernel_size, _old_ramdisk_size, os_version, header_size = struct.unpack_from(
        "<4I", base, 8
    )
    reserved = struct.unpack_from("<4I", base, 24)
    header_version = struct.unpack_from("<I", base, 40)[0]
    cmdline = base[44 : 44 + 1536]
    if header_version != 3 or header_size != 1580:
        raise ValueError(
            f"unsupported boot header: version={header_version} size={header_size}"
        )

    kernel_offset = align(header_size)
    kernel = base[kernel_offset : kernel_offset + kernel_size]

    header = bytearray()
    header += BOOT_MAGIC
    header += struct.pack("<4I", len(kernel), len(ramdisk), os_version, header_size)
    header += struct.pack("<4I", *reserved)
    header += struct.pack("<I", header_version)
    header += cmdline
    if len(header) != header_size:
        raise ValueError(f"unexpected header length: {len(header)}")

    def pad(blob):
        return blob + b"\0" * ((PAGE_SIZE - len(blob) % PAGE_SIZE) % PAGE_SIZE)

    out_path.write_bytes(pad(bytes(header)) + pad(kernel) + pad(ramdisk))


def main(argv):
    parser = argparse.ArgumentParser(
        description="Repack an Android boot v3 image with a valid gzip newc ramdisk."
    )
    parser.add_argument("base_boot_img", type=Path)
    parser.add_argument("ramdisk_dir", type=Path)
    parser.add_argument("out_boot_img", type=Path)
    parser.add_argument("--out-ramdisk", type=Path)
    args = parser.parse_args(argv)

    raw = build_newc(args.ramdisk_dir)
    if raw[:6] != b"070701":
        raise SystemExit("internal error: generated ramdisk is not newc at offset 0")
    ramdisk = gzip_ramdisk(raw)

    if args.out_ramdisk:
        args.out_ramdisk.parent.mkdir(parents=True, exist_ok=True)
        args.out_ramdisk.write_bytes(ramdisk)

    args.out_boot_img.parent.mkdir(parents=True, exist_ok=True)
    pack_boot_image(args.base_boot_img, ramdisk, args.out_boot_img)
    print(
        f"raw_newc_size={len(raw)} ramdisk_size={len(ramdisk)} "
        f"output={args.out_boot_img}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
