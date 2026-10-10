# Native bitmap fonts

The production program requires `gbk12-native.bin` and `gbk16-native.bin`
in `assets/` beside the executable. CMake copies both tables. Missing or
structurally invalid tables stop startup before entering the game loop.
Runtime uses C and SDL2; PRG, NAND, Python and a system font are not required.

## Current native assets (2026-10-10)

Both assets use only original NP560 V3.80 resident / flash glyphs. The
extractor reads the original unswapped PRG and unchanged header-inclusive
NAND_IMAGE upgrade slot. It does not use the historical Noto table.

- PRG SHA-256: `d555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5`.
- NAND_IMAGE SHA-256: `44c3c0332ceab48e95202effc1a6c4f52a07384c4149becf039384ea0c0330ce`.
- The original flash read address maps to source offset minus `0x110000`.
  All 727,550 font bytes match the factory NAND's tagged blocks after physical
  word-byte swapping; the 512-byte source header must remain present.

`gbk12-native.bin` retains `NPEFONT1`: 95 ASCII and 126 x 191 two-byte slots,
24 bytes per glyph, 579,872 bytes total. Original C font branches use resident
`A1..AF / A1..FE` punctuation and `B0..FD / A1..FE` Chinese data. Extended
`81..A0 / 40..FE` and `A8..FE / 40..A0` slots come from the original packed
18-byte flash glyphs; all 14,551 unpacked glyphs matched the original routine.
Other slots contain its resident A1A1 fallback. The previously used 7,709
resident glyphs remain byte-identical.
SHA-256: `a7b1d94bfb8edc3f082c2a8dfba064c57d00fe2a98b2a9a996af5c521f931391`.

`gbk16-native.bin` is `NPEF16V3`, 752,488 bytes. Its 95 ordinary ASCII,
8,836 standard slots and 33 control / DEL glyphs retain all V2 offsets and
bytes. An appended 14,551-glyph table contains the original 32-byte flash
rasters for the same extended ranges. Both native lookups include trail `7F`.
SHA-256: `c122ccf2562dc2c35f20ef0a6ea44f2c06ea8db1c4eaa669e2869f03ce598e43`.

The game title is read from the current NPE header; the fixed UI prefix and
labels use original glyphs. Existing original title, battle HUD, inventory
and title-load screen checks continue to pass. The new 150 C / actual SDL
glyph checks use captured original glyph buffers and positions, not original
full LCD or hardware timing references. See
[extraction and verification details](../../doc/games/np560_npe_extended_fonts.md).

From the repository root, regenerate with:

```text
python npe-sdl/tools/extract_np560_gbk_font.py tmp/NP560.PRG tmp/NP560_NAND_IMAGE.bin npe-sdl/assets/gbk12-native.bin npe-sdl/assets/gbk16-native.bin
cmake -S npe-sdl -B npe-sdl/build
```

## Compiled NPC special fonts

`../native_npc_fonts.inc` contains the original fonts8,9,23, selected by
NPC control bytes. Each has256 glyphs of8x16 pixels (one byte per row).
The12,288 data bytes are compiled into C; no additional runtime file is needed.
Their extraction and all768 original lookup checks are described in
[NPC special fonts](../../doc/games/np560_npe_npc_rich_text.md). Regenerate with:

```text
python npe-sdl/tools/extract_np560_npc_fonts.py tmp/NP560.PRG npe-sdl/native_npc_fonts.inc
```

## Historical generated font

`gbk12.bin` is retained as a historical development artifact. It is not copied
by CMake, loaded by production, or used to generate the native tables.

The historical `gbk12.bin` table was rasterized from
[`NotoSansCJKsc-Regular.otf`](https://github.com/notofonts/noto-cjk/blob/main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf)
in the official `notofonts/noto-cjk` repository. The source OTF SHA-256 is
`2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`.
The table SHA-256 is
`c66e0f55ebb59ce1f694f7ac140263f798f1e5396172f1853bf8fcdec43a2edc`.
The font and this derived table are distributed under the SIL Open Font License
1.1; the required license text is in [FONT-LICENSE.txt](FONT-LICENSE.txt).

To regenerate, download that exact OTF and run:

```text
python tools/generate_gbk_bitmap_font.py NotoSansCJKsc-Regular.otf assets/gbk12.bin
```

Python and Pillow are needed only for regeneration. Runtime rendering uses
`text.c` and SDL2.

The OFL statement above applies to the historical Noto source and derived
`gbk12.bin`. The native assets are extracted from the identified NP560 inputs.
