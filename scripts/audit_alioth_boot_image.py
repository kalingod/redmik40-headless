#!/usr/bin/env python3
import gzip
import struct
import sys
from pathlib import Path


PAGE_SIZE = 4096
BOOT_MAGIC = b"ANDROID!"
NEWC_MAGICS = (b"070701", b"070702")


def align(value):
    return (value + PAGE_SIZE - 1) // PAGE_SIZE * PAGE_SIZE


def parse_newc(raw):
    entries = {}
    offset = 0
    while offset + 110 <= len(raw):
        magic = raw[offset : offset + 6]
        if magic not in NEWC_MAGICS:
            raise ValueError(f"bad cpio magic at {offset}: {magic!r}")
        fields = [
            int(raw[offset + 6 + index * 8 : offset + 14 + index * 8], 16)
            for index in range(13)
        ]
        (
            _ino,
            mode,
            _uid,
            _gid,
            _nlink,
            _mtime,
            filesize,
            _devmajor,
            _devminor,
            rdevmajor,
            rdevminor,
            namesize,
            _check,
        ) = fields
        name_start = offset + 110
        name_end = name_start + namesize
        name = raw[name_start : name_end - 1].decode("utf-8", "replace")
        data_start = (name_end + 3) & ~3
        data_end = data_start + filesize
        entries[name] = (mode, raw[data_start:data_end], (rdevmajor, rdevminor))
        offset = (data_end + 3) & ~3
        if name == "TRAILER!!!":
            break
    return entries


def preview(data):
    if not data:
        return ""
    return data[:120].decode("utf-8", "replace").replace("\n", "\\n")


def audit(path):
    failures = []
    data = path.read_bytes()
    print(f"== {path} ==")

    if data[:8] != BOOT_MAGIC:
        print("boot: FAIL not an Android boot image")
        return 1

    kernel_size, ramdisk_size, os_version, header_size = struct.unpack_from(
        "<4I", data, 8
    )
    reserved = struct.unpack_from("<4I", data, 24)
    header_version = struct.unpack_from("<I", data, 40)[0]
    cmdline = data[44 : 44 + 1536].split(b"\0", 1)[0].decode(
        "utf-8", "replace"
    )
    kernel_offset = align(header_size)
    ramdisk_offset = kernel_offset + align(kernel_size)
    expected_total = ramdisk_offset + align(ramdisk_size)
    ramdisk = data[ramdisk_offset : ramdisk_offset + ramdisk_size]

    print(
        "boot: header_version={} header_size={} os_version={} "
        "kernel_size={} ramdisk_size={} total={}".format(
            header_version,
            header_size,
            os_version,
            kernel_size,
            ramdisk_size,
            len(data),
        )
    )
    print(
        "boot: reserved={} ramdisk_offset={} expected_total={} cmdline={!r}".format(
            reserved,
            ramdisk_offset,
            expected_total,
            cmdline,
        )
    )

    if header_version != 3:
        failures.append(f"unexpected header_version={header_version}")
    if header_size != 1580:
        failures.append(f"unexpected header_size={header_size}")
    if len(data) != expected_total:
        failures.append(f"unexpected image size: got {len(data)} expected {expected_total}")
    if ramdisk_size <= 0 or not ramdisk:
        failures.append("empty ramdisk")

    try:
        raw = gzip.decompress(ramdisk)
    except OSError as exc:
        failures.append(f"ramdisk gzip decode failed: {exc}")
        raw = b""

    if raw:
        magic8 = raw[:8]
        print(f"ramdisk: gzip_raw_size={len(raw)} first8={magic8!r}")
        if raw[:6] not in NEWC_MAGICS:
            failures.append(
                "ramdisk is not newc at offset 0; expected b'070701' or b'070702'"
            )
        else:
            try:
                entries = parse_newc(raw)
            except ValueError as exc:
                failures.append(str(exc))
            else:
                print(f"ramdisk: newc entries={len(entries)}")
                for name in (
                    "init",
                    "etc/alioth-init-mode",
                    "etc/alioth-ubuntu-rootfs",
                    "etc/alioth-systemd-wrapper-mode",
                    "etc/alioth-log0-marker",
                    "etc/alioth-log0b-newc-marker",
                    "etc/alioth-exp3-arch-ubuntu-rootfs",
                    "etc/alioth-exp3b-mac-ssh-key",
                ):
                    item = entries.get(name)
                    if item is None:
                        print(f"  {name}: MISSING")
                        continue
                    mode, content, rdev = item
                    print(
                        "  {}: mode={:o} size={} rdev={} preview={!r}".format(
                            name,
                            mode,
                            len(content),
                            rdev,
                            preview(content),
                        )
                    )

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        print()
        return 1

    print("OK")
    print()
    return 0


def main(argv):
    if len(argv) < 2:
        print(f"usage: {argv[0]} BOOT_IMG [BOOT_IMG ...]", file=sys.stderr)
        return 2

    status = 0
    for name in argv[1:]:
        path = Path(name)
        try:
            status |= audit(path)
        except OSError as exc:
            print(f"== {path} ==")
            print(f"FAIL: {exc}")
            print()
            status = 1
    return status


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
