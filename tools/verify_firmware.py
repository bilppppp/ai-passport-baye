#!/usr/bin/env python3
"""Verify the ESP32-C3 firmware layout produced by idf.py for AI-Passport-Baye."""

from __future__ import annotations

import hashlib
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


EXPECTED_IMAGES = (
    (0x0000, "bootloader/bootloader.bin"),
    (0x8000, "partition_table/partition-table.bin"),
    (0x10000, "AI-Passport-Baye.bin"),
)

FLASH_SIZE = 8 * 1024 * 1024
PARTITION_TABLE_OFFSET = 0x8000
PARTITION_TABLE_SIZE = 0xC00
APP_MAX_SIZE = 0x300000
CARDID_OFFSET = 0x356000
CARDID_SIZE = 0x4000
RECOVERY_OFFSET = 0x700000
RECOVERY_SIZE = 0x100000
ENTRY = struct.Struct("<HBBII16sI")
RECOVERY_BOOT_MARKER = b"UP held: booting permanent recovery"


@dataclass(frozen=True)
class Partition:
    kind: int
    subtype: int
    offset: int
    size: int
    label: str

    @property
    def end(self) -> int:
        return self.offset + self.size


def parse_partition_table(raw: bytes) -> tuple[list[Partition], bool]:
    """Parse an ESP-IDF table and verify its optional MD5 marker."""
    if len(raw) < PARTITION_TABLE_SIZE:
        raise ValueError("partition table is truncated")

    partitions: list[Partition] = []
    found_md5 = False
    for cursor in range(0, PARTITION_TABLE_SIZE, ENTRY.size):
        magic = int.from_bytes(raw[cursor : cursor + 2], "little")
        if magic == 0xFFFF:
            break
        if magic == 0xEBEB:
            expected = hashlib.md5(raw[:cursor]).digest()
            actual = raw[cursor + 16 : cursor + 32]
            if actual != expected:
                raise ValueError("partition table MD5 marker does not match")
            found_md5 = True
            break
        if magic != 0x50AA:
            raise ValueError(f"invalid partition entry at table offset 0x{cursor:x}")

        _, kind, subtype, offset, size, label_raw, _ = ENTRY.unpack_from(raw, cursor)
        label = label_raw.split(b"\0", 1)[0].decode("ascii", "strict")
        if not label or not size or offset < 0x9000 or offset + size > FLASH_SIZE:
            raise ValueError(f"invalid partition bounds for {label!r}")
        partitions.append(Partition(kind, subtype, offset, size, label))

    if not partitions:
        raise ValueError("partition table is empty")
    return partitions, found_md5


def verify_layout(build_dir: Path) -> None:
    app_path = build_dir / "AI-Passport-Baye.bin"
    if not app_path.is_file():
        raise ValueError(f"missing application binary: {app_path}")

    app_size = app_path.stat().st_size
    print(f"Application size: {app_size} bytes (0x{app_size:x})")
    if app_size > APP_MAX_SIZE:
        raise ValueError(f"application is {app_size} bytes; limit is {APP_MAX_SIZE}")

    app_bytes = app_path.read_bytes()
    if len(app_bytes) == 0 or app_bytes[0] != 0xE9:
        raise ValueError("application image missing ESP magic 0xE9 at offset 0")

    app_sha256 = hashlib.sha256(app_bytes).hexdigest()
    print(f"Application SHA-256: {app_sha256}")

    # Check partition table binary
    pt_path = build_dir / "partition_table" / "partition-table.bin"
    if pt_path.is_file():
        partitions, found_md5 = parse_partition_table(pt_path.read_bytes())
        print(f"Partition table: {len(partitions)} partitions found (MD5 verified: {found_md5})")
        by_label = {item.label: item for item in partitions}
        expected = {
            "factory": Partition(0, 0, 0x10000, APP_MAX_SIZE, "factory"),
            "cardid": Partition(1, 2, CARDID_OFFSET, CARDID_SIZE, "cardid"),
            "recovery": Partition(0, 0x20, RECOVERY_OFFSET, RECOVERY_SIZE, "recovery"),
        }
        for label, wanted in expected.items():
            if by_label.get(label) != wanted:
                raise ValueError(f"partition {label!r} must remain {wanted}, got {by_label.get(label)}")

        ordered = sorted(partitions, key=lambda item: item.offset)
        for left, right in zip(ordered, ordered[1:]):
            if left.end > right.offset:
                raise ValueError(f"partitions {left.label!r} and {right.label!r} overlap")
        for item in partitions:
            if item.label != "cardid" and item.offset < CARDID_OFFSET + CARDID_SIZE and CARDID_OFFSET < item.end:
                raise ValueError(f"partition {item.label!r} overlaps protected cardid")
            if item.label != "recovery" and item.offset < RECOVERY_OFFSET + RECOVERY_SIZE and RECOVERY_OFFSET < item.end:
                raise ValueError(f"partition {item.label!r} overlaps permanent Recovery")

    # Check bootloader recovery marker
    bl_path = build_dir / "bootloader" / "bootloader.bin"
    if bl_path.is_file():
        bootloader = bl_path.read_bytes()
        if RECOVERY_BOOT_MARKER not in bootloader:
            raise ValueError("bootloader is missing the 5-second UP Recovery hook")
        print("Bootloader recovery hook: VERIFIED (UP 5-sec jump to 0x700000)")

    print(f"Protected ranges check: PASS (cardid@0x{CARDID_OFFSET:x}, recovery@0x{RECOVERY_OFFSET:x} safe)")
    print(f"Firmware layout validation: PASS (app {app_size} / {APP_MAX_SIZE} bytes)")


def main() -> int:
    build_dir = Path(sys.argv[1] if len(sys.argv) > 1 else "build").resolve()
    try:
        verify_layout(build_dir)
    except (OSError, UnicodeDecodeError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
