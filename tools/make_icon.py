#!/usr/bin/env python3
"""Regenerate cpp/res/whitehole.ico from the repository artwork.

The checked-in .ico is committed so a normal build never needs Pillow.
Run this only when the artwork changes:

    python tools/make_icon.py
"""

from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "cpp" / "res" / "whitehole.ico"
SOURCES = [ROOT / "iconRAW.png", ROOT / "src" / "res" / "icon64.png"]
SIZES = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]


def main() -> int:
    try:
        from PIL import Image  # noqa: PLC0415 - optional build-time dependency
    except ImportError:
        print("Pillow is required to regenerate the icon: python -m pip install pillow", file=sys.stderr)
        return 1

    source = next((path for path in SOURCES if path.exists()), None)
    if source is None:
        print("No icon source artwork found (iconRAW.png)", file=sys.stderr)
        return 1

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    image = Image.open(source).convert("RGBA")
    image.save(OUTPUT, format="ICO", sizes=SIZES)
    print(f"wrote {OUTPUT.relative_to(ROOT)} ({OUTPUT.stat().st_size} bytes) from {source.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
