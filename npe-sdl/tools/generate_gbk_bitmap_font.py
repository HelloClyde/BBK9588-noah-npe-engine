"""Build the standalone 12x12 GBK bitmap used by the C/SDL text renderer.

The input OTF is Noto Sans CJK SC Regular from notofonts/noto-cjk (OFL 1.1).
The source font is a build-time input only; the runtime reads the generated
bitmap table. Keep assets/FONT-LICENSE.txt with the generated file.
"""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


GLYPH_SIZE = 12
GLYPH_BYTES = GLYPH_SIZE * 2
ASCII_COUNT = 95
GBK_LEADS = 126
GBK_TRAILS = 191


def raster(font: ImageFont.FreeTypeFont, character: str) -> bytes:
    image = Image.new("L", (GLYPH_SIZE, GLYPH_SIZE))
    if character != " ":
        bounds = font.getbbox(character)
        if bounds:
            left, top, right, bottom = bounds
            x = (GLYPH_SIZE - (right - left)) // 2 - left
            y = (GLYPH_SIZE - (bottom - top)) // 2 - top
            ImageDraw.Draw(image).text((x, y), character, font=font, fill=255)
    output = bytearray()
    for y in range(GLYPH_SIZE):
        bits = 0
        for x in range(GLYPH_SIZE):
            if image.getpixel((x, y)) >= 96:
                bits |= 1 << (15 - x)
        output.extend(bits.to_bytes(2, "big"))
    return bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("font", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    font = ImageFont.truetype(str(args.font), GLYPH_SIZE)
    table = bytearray(b"NPEFONT1")
    for value in range(0x20, 0x7F):
        table.extend(raster(font, chr(value)))
    for lead in range(0x81, 0xFF):
        for trail in range(0x40, 0xFF):
            try:
                character = bytes((lead, trail)).decode("gbk")
            except UnicodeDecodeError:
                character = " "
            table.extend(raster(font, character))
    expected = 8 + (ASCII_COUNT + GBK_LEADS * GBK_TRAILS) * GLYPH_BYTES
    assert len(table) == expected
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(table)
    print(f"{args.output}: {len(table)} bytes")


if __name__ == "__main__":
    main()
