#!/usr/bin/env python3
import ctypes
import errno
import fcntl
import os
import stat
import struct
import sys
import time


KGSL_NODE = "/dev/kgsl-3d0"
KGSL_SYSFS = "/sys/class/kgsl/kgsl-3d0"

KGSL_IOC_TYPE = 0x09
KGSL_PROP_DEVICE_INFO = 0x1
KGSL_PROP_VERSION = 0x8
KGSL_PROP_UCHE_GMEM_VADDR = 0x13
KGSL_PROP_DEVICE_BITNESS = 0x18
KGSL_PROP_UBWC_MODE = 0x1B

_IOC_NRBITS = 8
_IOC_TYPEBITS = 8
_IOC_SIZEBITS = 14
_IOC_DIRBITS = 2

_IOC_NRSHIFT = 0
_IOC_TYPESHIFT = _IOC_NRSHIFT + _IOC_NRBITS
_IOC_SIZESHIFT = _IOC_TYPESHIFT + _IOC_TYPEBITS
_IOC_DIRSHIFT = _IOC_SIZESHIFT + _IOC_SIZEBITS

_IOC_WRITE = 1
_IOC_READ = 2


class KgslDeviceGetProperty(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_uint),
        ("value", ctypes.c_void_p),
        ("sizebytes", ctypes.c_size_t),
    ]


class KgslVersion(ctypes.Structure):
    _fields_ = [
        ("drv_major", ctypes.c_uint),
        ("drv_minor", ctypes.c_uint),
        ("dev_major", ctypes.c_uint),
        ("dev_minor", ctypes.c_uint),
    ]


class KgslDevInfo(ctypes.Structure):
    _fields_ = [
        ("device_id", ctypes.c_uint),
        ("chip_id", ctypes.c_uint),
        ("mmu_enabled", ctypes.c_uint),
        ("gmem_gpubaseaddr", ctypes.c_ulong),
        ("gpu_id", ctypes.c_uint),
        ("gmem_sizebytes", ctypes.c_size_t),
    ]


def _ioc(direction, ioctl_type, number, size):
    return (
        (direction << _IOC_DIRSHIFT)
        | (ioctl_type << _IOC_TYPESHIFT)
        | (number << _IOC_NRSHIFT)
        | (size << _IOC_SIZESHIFT)
    )


def _iowr(ioctl_type, number, ctype):
    return _ioc(_IOC_READ | _IOC_WRITE, ioctl_type, number, ctypes.sizeof(ctype))


IOCTL_KGSL_DEVICE_GETPROPERTY = _iowr(
    KGSL_IOC_TYPE, 0x2, KgslDeviceGetProperty
)


def read_text(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            return handle.read().strip()
    except OSError as exc:
        return f"<{exc.__class__.__name__}: {exc}>"


def dump_node_state(label):
    print(f"[{label}]")
    try:
        st = os.stat(KGSL_NODE)
        print(
            "node: path={} mode={} uid={} gid={} rdev_major={} rdev_minor={}".format(
                KGSL_NODE,
                stat.filemode(st.st_mode),
                st.st_uid,
                st.st_gid,
                os.major(st.st_rdev),
                os.minor(st.st_rdev),
            )
        )
    except OSError as exc:
        print(f"node: path={KGSL_NODE} error={exc.errno} {exc.strerror}")

    print(f"sysfs: path={KGSL_SYSFS} realpath={os.path.realpath(KGSL_SYSFS)}")
    for name in (
        "dev",
        "gpu_model",
        "gpu_busy_percentage",
        "gpubusy",
        "gpuclk",
        "max_gpuclk",
        "pwrlevel",
    ):
        print(f"sysfs.{name}: {read_text(os.path.join(KGSL_SYSFS, name))}")


def ioctl_getproperty(fd, prop_type, value):
    arg = bytearray(
        struct.pack(
            "@I4xQQ",
            prop_type,
            ctypes.addressof(value),
            ctypes.sizeof(value),
        )
    )
    fcntl.ioctl(fd, IOCTL_KGSL_DEVICE_GETPROPERTY, arg, True)
    return value


def prop_or_die(fd, name, prop_type, value):
    try:
        result = ioctl_getproperty(fd, prop_type, value)
    except OSError as exc:
        print(f"{name}: FAIL errno={exc.errno} strerror={exc.strerror}")
        raise
    return result


def main():
    print("kgsl_getproperty_probe_version=1")
    print(f"python={sys.version.split()[0]} executable={sys.executable}")
    print(f"pid={os.getpid()} uid={os.getuid()} gid={os.getgid()} groups={os.getgroups()}")
    print(f"time_epoch={int(time.time())}")
    print(f"ctypes.sizeof(KgslDeviceGetProperty)={ctypes.sizeof(KgslDeviceGetProperty)}")
    print(f"ioctl_getproperty=0x{IOCTL_KGSL_DEVICE_GETPROPERTY:08x}")

    dump_node_state("before")

    flags = os.O_RDWR | getattr(os, "O_CLOEXEC", 0)
    try:
        fd = os.open(KGSL_NODE, flags)
    except OSError as exc:
        print(f"open: FAIL path={KGSL_NODE} errno={exc.errno} strerror={exc.strerror}")
        print("overall=FAIL")
        return 1

    print(f"open: OK path={KGSL_NODE} fd={fd} flags=O_RDWR|O_CLOEXEC")

    try:
        version = prop_or_die(fd, "KGSL_PROP_VERSION", KGSL_PROP_VERSION, KgslVersion())
        print(
            "KGSL_PROP_VERSION: OK drv={}.{} dev={}.{}".format(
                version.drv_major,
                version.drv_minor,
                version.dev_major,
                version.dev_minor,
            )
        )

        devinfo = prop_or_die(
            fd, "KGSL_PROP_DEVICE_INFO", KGSL_PROP_DEVICE_INFO, KgslDevInfo()
        )
        derived_gpu_id = (
            ((devinfo.chip_id >> 24) & 0xFF) * 100
            + ((devinfo.chip_id >> 16) & 0xFF) * 10
            + ((devinfo.chip_id >> 8) & 0xFF)
        )
        print(
            "KGSL_PROP_DEVICE_INFO: OK device_id={} chip_id=0x{:08x} "
            "mmu_enabled={} gmem_gpubaseaddr=0x{:x} gpu_id={} "
            "derived_gpu_id={} gmem_sizebytes={}".format(
                devinfo.device_id,
                devinfo.chip_id,
                devinfo.mmu_enabled,
                int(devinfo.gmem_gpubaseaddr),
                devinfo.gpu_id,
                derived_gpu_id,
                int(devinfo.gmem_sizebytes),
            )
        )

        gmem_iova = prop_or_die(
            fd,
            "KGSL_PROP_UCHE_GMEM_VADDR",
            KGSL_PROP_UCHE_GMEM_VADDR,
            ctypes.c_uint64(),
        )
        print(f"KGSL_PROP_UCHE_GMEM_VADDR: OK value=0x{gmem_iova.value:x}")

        bitness = prop_or_die(
            fd,
            "KGSL_PROP_DEVICE_BITNESS",
            KGSL_PROP_DEVICE_BITNESS,
            ctypes.c_uint(),
        )
        print(f"KGSL_PROP_DEVICE_BITNESS: OK value={bitness.value}")

        ubwc_mode = prop_or_die(
            fd,
            "KGSL_PROP_UBWC_MODE",
            KGSL_PROP_UBWC_MODE,
            ctypes.c_uint(),
        )
        print(f"KGSL_PROP_UBWC_MODE: OK value={ubwc_mode.value}")
    except OSError:
        os.close(fd)
        dump_node_state("after-fail")
        print("overall=FAIL")
        return 2

    os.close(fd)
    dump_node_state("after")
    print("overall=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
