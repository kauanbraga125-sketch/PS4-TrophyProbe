#!/usr/bin/env python3
from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path

W = H = 512


def png_chunk(kind: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(kind + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", crc)


def rgba(x: int, y: int) -> tuple[int, int, int, int]:
    # Dark background.
    r, g, b = 18, 22, 31

    # Large probe/crosshair motif.
    cx, cy = 256, 256
    dx, dy = x - cx, y - cy
    d2 = dx * dx + dy * dy

    if 160 * 160 <= d2 <= 175 * 175:
        r, g, b = 72, 150, 235
    if 72 * 72 <= d2 <= 88 * 88:
        r, g, b = 235, 240, 247
    if abs(dx) <= 10 and 92 <= abs(dy) <= 205:
        r, g, b = 72, 150, 235
    if abs(dy) <= 10 and 92 <= abs(dx) <= 205:
        r, g, b = 72, 150, 235
    if d2 <= 24 * 24:
        r, g, b = 245, 193, 78

    return r, g, b, 255


def build_png(path: str) -> None:
    raw = bytearray()
    for y in range(H):
        raw.append(0)
        for x in range(W):
            raw.extend(rgba(x, y))

    out = b"\x89PNG\r\n\x1a\n"
    out += png_chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0))
    out += png_chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    out += png_chunk(b"IEND", b"")
    Path(path).write_bytes(out)


if __name__ == "__main__":
    build_png(sys.argv[1])
