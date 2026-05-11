#!/usr/bin/env python3
import argparse
import ctypes
import os
import struct
import time


EV_FF = 0x15
FF_RUMBLE = 0x50
FF_CONSTANT = 0x52

IOC_NRBITS = 8
IOC_TYPEBITS = 8
IOC_SIZEBITS = 14
IOC_DIRBITS = 2

IOC_NRSHIFT = 0
IOC_TYPESHIFT = IOC_NRSHIFT + IOC_NRBITS
IOC_SIZESHIFT = IOC_TYPESHIFT + IOC_TYPEBITS
IOC_DIRSHIFT = IOC_SIZESHIFT + IOC_SIZEBITS

IOC_WRITE = 1


class FFTrigger(ctypes.Structure):
    _fields_ = [
        ("button", ctypes.c_uint16),
        ("interval", ctypes.c_uint16),
    ]


class FFReplay(ctypes.Structure):
    _fields_ = [
        ("length", ctypes.c_uint16),
        ("delay", ctypes.c_uint16),
    ]


class FFEnvelope(ctypes.Structure):
    _fields_ = [
        ("attack_length", ctypes.c_uint16),
        ("attack_level", ctypes.c_uint16),
        ("fade_length", ctypes.c_uint16),
        ("fade_level", ctypes.c_uint16),
    ]


class FFConstantEffect(ctypes.Structure):
    _fields_ = [
        ("level", ctypes.c_int16),
        ("envelope", FFEnvelope),
    ]


class FFRampEffect(ctypes.Structure):
    _fields_ = [
        ("start_level", ctypes.c_int16),
        ("end_level", ctypes.c_int16),
        ("envelope", FFEnvelope),
    ]


class FFPeriodicEffect(ctypes.Structure):
    _fields_ = [
        ("waveform", ctypes.c_uint16),
        ("period", ctypes.c_uint16),
        ("magnitude", ctypes.c_int16),
        ("offset", ctypes.c_int16),
        ("phase", ctypes.c_uint16),
        ("envelope", FFEnvelope),
        ("custom_len", ctypes.c_uint32),
        ("custom_data", ctypes.POINTER(ctypes.c_int16)),
    ]


class FFConditionEffect(ctypes.Structure):
    _fields_ = [
        ("right_saturation", ctypes.c_uint16),
        ("left_saturation", ctypes.c_uint16),
        ("right_coeff", ctypes.c_int16),
        ("left_coeff", ctypes.c_int16),
        ("deadband", ctypes.c_uint16),
        ("center", ctypes.c_int16),
    ]


class FFRumbleEffect(ctypes.Structure):
    _fields_ = [
        ("strong_magnitude", ctypes.c_uint16),
        ("weak_magnitude", ctypes.c_uint16),
    ]


class FFEffectUnion(ctypes.Union):
    _fields_ = [
        ("constant", FFConstantEffect),
        ("ramp", FFRampEffect),
        ("periodic", FFPeriodicEffect),
        ("condition", FFConditionEffect * 2),
        ("rumble", FFRumbleEffect),
    ]


class FFEffect(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_uint16),
        ("id", ctypes.c_int16),
        ("direction", ctypes.c_uint16),
        ("trigger", FFTrigger),
        ("replay", FFReplay),
        ("u", FFEffectUnion),
    ]


def ioc(direction, type_char, nr, size):
    return (
        (direction << IOC_DIRSHIFT)
        | (ord(type_char) << IOC_TYPESHIFT)
        | (nr << IOC_NRSHIFT)
        | (size << IOC_SIZESHIFT)
    )


def write_ff_event(fd, code, value):
    if ctypes.sizeof(ctypes.c_long) == 8:
        event = struct.pack("qqHHi", 0, 0, EV_FF, code, value)
    else:
        event = struct.pack("llHHi", 0, 0, EV_FF, code, value)
    os.write(fd, event)


def ioctl_ptr(fd, request, obj):
    libc = ctypes.CDLL(None, use_errno=True)
    libc.ioctl.argtypes = [ctypes.c_int, ctypes.c_ulong, ctypes.c_void_p]
    libc.ioctl.restype = ctypes.c_int
    rc = libc.ioctl(
        ctypes.c_int(fd),
        ctypes.c_ulong(request),
        ctypes.c_void_p(ctypes.addressof(obj)),
    )
    if rc < 0:
        err = ctypes.get_errno()
        raise OSError(err, os.strerror(err))
    return rc


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("device", nargs="?", default="/dev/input/event4")
    parser.add_argument("--effect", choices=("constant", "rumble"), default="constant")
    parser.add_argument("--length-ms", type=int, default=120)
    parser.add_argument("--level", type=lambda s: int(s, 0), default=0x3000)
    parser.add_argument("--strong", type=lambda s: int(s, 0), default=0x3000)
    parser.add_argument("--weak", type=lambda s: int(s, 0), default=0)
    args = parser.parse_args()

    effect = FFEffect()
    effect.type = FF_CONSTANT if args.effect == "constant" else FF_RUMBLE
    effect.id = -1
    effect.direction = 0
    effect.trigger.button = 0
    effect.trigger.interval = 0
    effect.replay.length = max(1, min(args.length_ms, 1000))
    effect.replay.delay = 0
    if args.effect == "constant":
        effect.u.constant.level = max(-0x7FFF, min(args.level, 0x7FFF))
    else:
        effect.u.rumble.strong_magnitude = max(0, min(args.strong, 0xFFFF))
        effect.u.rumble.weak_magnitude = max(0, min(args.weak, 0xFFFF))

    evioscff = ioc(IOC_WRITE, "E", 0x80, ctypes.sizeof(FFEffect))
    eviocrmff = ioc(IOC_WRITE, "E", 0x81, ctypes.sizeof(ctypes.c_int))

    fd = os.open(args.device, os.O_RDWR | os.O_CLOEXEC)
    try:
        print(f"device={args.device}")
        print(f"effect={args.effect} type=0x{effect.type:02x}")
        print(f"ff_effect_size={ctypes.sizeof(FFEffect)} evioscff=0x{evioscff:x}")
        ioctl_ptr(fd, evioscff, effect)
        print(f"uploaded_effect_id={effect.id}")
        write_ff_event(fd, effect.id, 1)
        if args.effect == "constant":
            print(f"play_ms={effect.replay.length} level={effect.u.constant.level}")
        else:
            print(f"play_ms={effect.replay.length} strong=0x{effect.u.rumble.strong_magnitude:04x} weak=0x{effect.u.rumble.weak_magnitude:04x}")
        time.sleep((effect.replay.length + 80) / 1000.0)
        write_ff_event(fd, effect.id, 0)
        effect_id = ctypes.c_int(effect.id)
        try:
            ioctl_ptr(fd, eviocrmff, effect_id)
            print("removed_effect=ok")
        except OSError as exc:
            print(f"removed_effect=warn errno={exc.errno} {exc.strerror}")
    finally:
        os.close(fd)


if __name__ == "__main__":
    main()
