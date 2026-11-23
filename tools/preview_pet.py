#!/usr/bin/env python3
"""Render the compiled VOLT pet sprites to PNG previews; Python stdlib only.

Two images are produced from composePet()/petFrame() compiled with the host
C++11 compiler (the same source the firmware uses):
  * a real-size sheet: every animation sample x every style at 2x = 96x96 px,
    exactly the pixel size drawn on the 320x240 CYD panel;
  * a zoom sheet: the CLASSIC samples at 4x to inspect individual cells.
This previews sprite art and RGB565 colours, not the TFT hardware, SPI timing,
UI effects (hearts, Z, sparks) or touch. No board, Serial port, Wi-Fi
credentials or token is accessed.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import zlib

ROOT = Path(__file__).resolve().parents[1]
SIZE, SCALE, ZOOM = 48, 2, 4
FONT = {
    'A': (0x7E, 0x11, 0x11, 0x11, 0x7E), 'B': (0x7F, 0x49, 0x49, 0x49, 0x36),
    'C': (0x3E, 0x41, 0x41, 0x41, 0x22), 'D': (0x7F, 0x41, 0x41, 0x22, 0x1C),
    'E': (0x7F, 0x49, 0x49, 0x49, 0x41), 'F': (0x7F, 0x09, 0x09, 0x09, 0x01),
    'G': (0x3E, 0x41, 0x49, 0x49, 0x7A), 'H': (0x7F, 0x08, 0x08, 0x08, 0x7F),
    'I': (0x00, 0x41, 0x7F, 0x41, 0x00), 'J': (0x20, 0x40, 0x41, 0x3F, 0x01),
    'K': (0x7F, 0x08, 0x14, 0x22, 0x41), 'L': (0x7F, 0x40, 0x40, 0x40, 0x40),
    'M': (0x7F, 0x02, 0x0C, 0x02, 0x7F), 'N': (0x7F, 0x04, 0x08, 0x10, 0x7F),
    'O': (0x3E, 0x41, 0x41, 0x41, 0x3E), 'P': (0x7F, 0x09, 0x09, 0x09, 0x06),
    'Q': (0x3E, 0x41, 0x51, 0x21, 0x5E), 'R': (0x7F, 0x09, 0x19, 0x29, 0x46),
    'S': (0x46, 0x49, 0x49, 0x49, 0x31), 'T': (0x01, 0x01, 0x7F, 0x01, 0x01),
    'U': (0x3F, 0x40, 0x40, 0x40, 0x3F), 'V': (0x1F, 0x20, 0x40, 0x20, 0x1F),
    'W': (0x3F, 0x40, 0x38, 0x40, 0x3F), 'X': (0x63, 0x14, 0x08, 0x14, 0x63),
    'Y': (0x07, 0x08, 0x70, 0x08, 0x07), 'Z': (0x61, 0x51, 0x49, 0x45, 0x43),
    '0': (0x3E, 0x51, 0x49, 0x45, 0x3E), '1': (0x00, 0x42, 0x7F, 0x40, 0x00),
    '2': (0x42, 0x61, 0x51, 0x49, 0x46), '3': (0x21, 0x41, 0x45, 0x4B, 0x31),
    '4': (0x18, 0x14, 0x12, 0x7F, 0x10), '5': (0x27, 0x45, 0x45, 0x45, 0x39),
    '6': (0x3C, 0x4A, 0x49, 0x49, 0x30), '7': (0x01, 0x71, 0x09, 0x05, 0x03),
    '8': (0x36, 0x49, 0x49, 0x49, 0x36), '9': (0x06, 0x49, 0x49, 0x29, 0x1E),
    '-': (0x08, 0x08, 0x08, 0x08, 0x08), ' ': (0, 0, 0, 0, 0),
}
SAMPLES = ['IDLE', 'BREATHE', 'BLINK', 'HAPPY A', 'HAPPY B', 'SLEEP A', 'SLEEP B', 'LEVEL UP A', 'LEVEL UP B',
           'TICKLE A', 'TICKLE B', 'GRAPPLE LEFT', 'GRAPPLE RIGHT', 'HANG A', 'HANG B']
DUMP_SOURCE = r'''
#include <cstdio>
#include "ui/PetSprites.h"
using namespace deskos;
int main() {
  struct Sample { PetMood mood; PetReaction reaction; uint32_t elapsed,anim; };
  const PetReaction N=PetReaction::None;
  const Sample samples[]={
    {PetMood::Idle,N,0,0}, {PetMood::Idle,N,600,600}, {PetMood::Idle,N,3100,0},
    {PetMood::Happy,N,0,0}, {PetMood::Happy,N,250,250},
    {PetMood::Sleep,N,0,0}, {PetMood::Sleep,N,900,900},
    {PetMood::Celebrate,N,0,0}, {PetMood::Celebrate,N,250,250},
    {PetMood::Idle,PetReaction::Tickle,0,0}, {PetMood::Idle,PetReaction::Tickle,150,150},
    {PetMood::Idle,PetReaction::GrappleLeft,0,0}, {PetMood::Idle,PetReaction::GrappleRight,0,0},
    {PetMood::Idle,PetReaction::Hang,0,0}, {PetMood::Idle,PetReaction::Hang,400,400}
  };
  for (const auto& sample:samples) for (uint8_t s=0;s<PET_STYLE_COUNT;++s) {
    const PetStyle style=static_cast<PetStyle>(s);
    PetCanvas canvas;
    composePet(canvas,petFrame(sample.mood,sample.reaction,style,sample.elapsed,sample.elapsed,sample.anim));
    std::printf("%s\n",styleName(style));
    for (const auto& row:canvas.rows) std::puts(row);
  }
}
'''


def from565(value):
    return bytes((((value >> 11) & 31) * 255 // 31, ((value >> 5) & 63) * 255 // 63, (value & 31) * 255 // 31))


def rgb565(rgb):
    r, g, b = rgb
    return from565(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))


def palette():
    """Sprite inks from the generated header (exact RGB565 the panel receives) + UI text colours."""
    theme = (ROOT / 'src/ui/UITheme.h').read_text(encoding='utf-8')
    data = (ROOT / 'src/ui/PetSpriteData.h').read_text(encoding='utf-8')
    literal = r'rgb565[(]0x([0-9A-Fa-f]+),0x([0-9A-Fa-f]+),0x([0-9A-Fa-f]+)[)]'
    result = {}
    for key, name in (('.', 'Background'), ('G', 'Gold'), ('m', 'Mint')):
        match = re.search(name + '=' + literal, theme)
        result[key] = rgb565(tuple(int(v, 16) for v in match.groups()))
    result['T'] = bytes((255, 255, 255))
    inks = re.findall(r"[{]'(.)',0x([0-9A-Fa-f]{4})[}]", data)
    if len(inks) < 8:
        raise ValueError('Update preview palette parsing for the changed PetSpriteData.h')
    for key, value in inks:
        result[key] = from565(int(value, 16))
    return result


class Image:
    def __init__(self, width, height, background):
        self.width, self.height = width, height
        self.pixels = bytearray(background * (width * height))

    def rect(self, x, y, w, h, colour):
        assert 0 <= x <= x + w <= self.width and 0 <= y <= y + h <= self.height
        for yy in range(y, y + h):
            start = (yy * self.width + x) * 3
            self.pixels[start:start + w * 3] = colour * w

    def text(self, value, x, y, colour, scale=1):
        for letter in value:
            for col, bits in enumerate(FONT[letter]):
                for row in range(7):
                    if bits & (1 << row):
                        self.rect(x + col * scale, y + row * scale, scale, scale, colour)
            x += 6 * scale

    def sprite(self, rows, x, y, scale, colours):
        for yy, cells in enumerate(rows):
            for xx, key in enumerate(cells):
                self.rect(x + xx * scale, y + yy * scale, scale, scale, colours[key])

    def save(self, output):
        def chunk(tag, data):
            return struct.pack('!I', len(data)) + tag + data + struct.pack('!I', zlib.crc32(tag + data))
        row_bytes = self.width * 3
        raw = b''.join(b'\0' + self.pixels[y * row_bytes:(y + 1) * row_bytes] for y in range(self.height))
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(b'\x89PNG\r\n\x1a\n' +
                           chunk(b'IHDR', struct.pack('!2I5B', self.width, self.height, 8, 2, 0, 0, 0)) +
                           chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def render_real_size(frames, styles, output, colours):
    cell = SIZE * SCALE  # 96 px: the on-screen pet area.
    pitch_x, pitch_y, left, top = cell + 32, cell + 26, 96, 74
    image = Image(left + len(styles) * pitch_x, top + len(SAMPLES) * pitch_y, colours['.'])
    image.text('VOLT', 16, 12, colours['T'], 2)
    image.text('REAL SIZE - 48X48 SPRITE AT 2X - 96X96 PX', 150, 16, colours['G'])
    image.text('HOST PREVIEW FROM COMPILED FIRMWARE SPRITES - HARDWARE PENDING', 16, 36, colours['T'])
    for column, style in enumerate(styles):
        image.text(style, left + column * pitch_x + (cell - len(style) * 6) // 2, 56, colours['G'])
    for row, label in enumerate(SAMPLES):
        image.text(label, 8, top + row * pitch_y + cell // 2 - 4, colours['m'])
        for column, style in enumerate(styles):
            image.sprite(frames[row * len(styles) + column], left + column * pitch_x, top + row * pitch_y,
                         SCALE, colours)
    image.save(output)


def render_zoom(frames, styles, output, colours):
    cell, columns = SIZE * ZOOM, 5
    pitch_x, pitch_y, top = cell + 24, cell + 30, 46
    image = Image(16 + columns * pitch_x, top + ((len(SAMPLES) + columns - 1) // columns) * pitch_y, colours['.'])
    image.text('VOLT CLASSIC - ZOOM 4X', 16, 12, colours['T'], 2)
    for index, label in enumerate(SAMPLES):
        x, y = 16 + (index % columns) * pitch_x, top + (index // columns) * pitch_y
        image.text(label, x, y, colours['m'])
        image.sprite(frames[index * len(styles)], x, y + 14, ZOOM, colours)
    image.save(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/assets/pet_preview.png')
    parser.add_argument('--zoom-output', type=Path, default=ROOT / 'docs/assets/pet_zoom.png')
    args = parser.parse_args()
    compiler = args.compiler or ROOT / '.pio/host-tools/toolchain-gccmingw32/bin/g++.exe'
    if not Path(compiler).is_file():
        compiler = shutil.which('g++')
    if not compiler:
        parser.error('Pass --compiler with a C++11 host compiler')
    build = ROOT / '.pio/pet_preview'
    build.mkdir(parents=True, exist_ok=True)
    exe = build / 'pet_preview.exe'
    compiler_env = dict(os.environ)
    compiler_env['PATH'] = str(Path(compiler).parent) + os.pathsep + compiler_env.get('PATH', '')
    subprocess.run([str(compiler), '-std=c++11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-static',
                    '-Isrc', '-x', 'c++', '-', '-x', 'none', 'src/ui/PetSprites.cpp', 'src/services/PetModel.cpp',
                    '-o', str(exe)], input=DUMP_SOURCE, text=True, cwd=ROOT, env=compiler_env, check=True)
    lines = subprocess.check_output([str(exe)], text=True, cwd=ROOT, env=compiler_env).splitlines()
    frames, styles = [], []
    for offset in range(0, len(lines), SIZE + 1):
        style, rows = lines[offset], lines[offset + 1:offset + SIZE + 1]
        if len(rows) != SIZE or any(len(row) != SIZE for row in rows):
            raise ValueError('Update preview dimensions for the changed sprite size')
        if style not in styles:
            styles.append(style)
        frames.append(rows)
    if len(styles) != 5 or len(frames) != len(SAMPLES) * len(styles):
        raise ValueError('Expected every animation sample across all 5 styles')
    colours = palette()
    render_real_size(frames, styles, args.output, colours)
    render_zoom(frames, styles, args.zoom_output, colours)
    print(f'Rendered {len(frames)} compiled sprite samples -> {args.output} and {args.zoom_output}')


if __name__ == '__main__':
    main()
