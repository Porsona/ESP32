#!/usr/bin/env python3
"""Generate the VOLT pet sprite data (src/ui/PetSpriteData.h); Python stdlib only.

VOLT is DeskOS's original mascot: a tiny front-facing robot hero in the DeskOS
Retro Pixel palette - mint helmet with a dark visor that shows LED eyes, gold
antenna and ear bolts, purple body with a glowing chest core, rose mitts and
gold boots. Shapes (ellipses, capsules, polygons) are rasterised and shaded
from one light direction, then emitted as constant character rows.

Runtime composition = pose frame + LED-eye overlay + optional crown overlay,
all offset by the pose's head position (the firmware builds the upside-down
"hang" frames by flipping the Cheer frame). This script asserts that the
composition equals a direct render for every pose/eyes/crown and that overlays
never cover an arm or the body.

  python tools/pet_art/generate_pet.py                 # rewrite the header
  python tools/pet_art/generate_pet.py --check         # fail if it is stale
  python tools/pet_art/generate_pet.py --preview a.png # quick look at every frame
"""
import argparse
import math
from pathlib import Path
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'src/ui/PetSpriteData.h'
W = H = 48
CX = 24.0  # vertical symmetry axis (between columns 23 and 24)
# Sprite characters and their colours ('.' = transparent, drawn as theme background).
PALETTE = [
    ('#', (0x0C, 0x0A, 0x14), 'outline'),
    ('h', (0xC8, 0xFF, 0xEA), 'mint highlight'),
    ('m', (0x72, 0xE6, 0xBA), 'mint (DeskOS accent)'),
    ('M', (0x3F, 0xA8, 0x8A), 'mint shadow'),
    ('v', (0x1B, 0x15, 0x30), 'visor'),
    ('e', (0x9C, 0xF6, 0xFF), 'LED glow'),
    ('p', (0x6A, 0x4F, 0x95), 'purple'),
    ('P', (0x43, 0x35, 0x5D), 'purple shadow (DeskOS panel)'),
    ('g', (0xF3, 0xD3, 0x7A), 'gold (DeskOS)'),
    ('o', (0xF2, 0x8F, 0xAB), 'rose (DeskOS)'),
    ('w', (0xFF, 0xFF, 0xFF), 'white: glints, cable'),
]
POSE_ORDER = ['Relaxed', 'Breathe', 'Wave', 'WaveFlick', 'Sleep', 'SleepBreathe', 'Cheer', 'CheerBounce',
              'Laugh', 'LaughShake', 'GrappleLeft', 'GrappleRight']
EYE_ORDER = ['Open', 'Closed', 'Happy', 'Sleepy', 'Star', 'Heart']
CABLE_HAND_Y = 30  # row of the grapple cable leaving the outstretched hand
LIGHT = (-0.5, -0.55, 0.67)
_norm = math.sqrt(sum(v * v for v in LIGHT))
LIGHT = tuple(v / _norm for v in LIGHT)


# ---------- shape helpers ----------
def ell(cx, cy, rx, ry):
    return lambda x, y: ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0


def squircle(cx, cy, rx, ry):
    return lambda x, y: ((x - cx) / rx) ** 4 + ((y - cy) / ry) ** 4 <= 1.0


def capsule(ax, ay, bx, by, r):
    def inside(x, y):
        vx, vy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((x - ax) * vx + (y - ay) * vy) / (vx * vx + vy * vy or 1)))
        return (x - ax - t * vx) ** 2 + (y - ay - t * vy) ** 2 <= r * r
    return inside


def poly(points):
    def inside(x, y):
        result = False
        for i in range(len(points)):
            (x1, y1), (x2, y2) = points[i], points[i - 1]
            if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
                result = not result
        return result
    return inside


def union(*masks):
    return lambda x, y: any(m(x, y) for m in masks)


def mirror_pt(p):
    return (2 * CX - p[0], p[1])


def shade(x, y, cx, cy, rx, ry):
    nx, ny = (x - cx) / rx, (y - cy) / ry
    d = nx * nx + ny * ny
    if d > 1:
        s = math.sqrt(d)
        nx, ny, d = nx / s, ny / s, 1.0
    nz = math.sqrt(max(0.0, 1 - d))
    return nx * LIGHT[0] + ny * LIGHT[1] + nz * LIGHT[2]


def mint(b):
    return 'M' if b < 0.3 else 'm' if b < 0.84 else 'h'


def purple(b):
    return 'P' if b < 0.45 else 'p'


class Canvas:
    def __init__(self):
        self.c = [['.'] * W for _ in range(H)]
        self.owner = [[None] * W for _ in range(H)]
        self.ox = self.oy = 0  # whole-pixel offset for overlays: shapes are evaluated in local coordinates

    def cells(self, mask):
        return {(x, y) for y in range(H) for x in range(W) if mask(x + 0.5 - self.ox, y + 0.5 - self.oy)}

    def put(self, x, y, ch, owner):
        if 0 <= x < W and 0 <= y < H:
            self.c[y][x] = ch
            self.owner[y][x] = owner

    def part(self, mask, colour, owner, outline=True):
        pts = self.cells(mask)
        for (x, y) in pts:
            edge = any((x + dx, y + dy) not in pts for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            self.put(x, y, '#' if outline and edge else colour(x + 0.5 - self.ox, y + 0.5 - self.oy), owner)

    def rows(self):
        return [''.join(r) for r in self.c]


# ---------- head: mint helmet, dark visor, antenna and ear bolts ----------
HC = (CX, 14.5)
VISOR = (CX, 15.6, 10.6, 6.2)  # squircle centre and radii at offset (0,0)
EYE_L = (18.6, 15.4)          # viewer's left LED eye centre; the right one is mirrored


def head(c, dx, dy):
    cx, cy = HC[0] + dx, HC[1] + dy
    tip = (11.8 + dx, 2.2 + dy)
    c.part(capsule(15.6 + dx, 6.8 + dy, tip[0], tip[1], 1.3), lambda x, y: 'M', 'head')
    c.part(ell(tip[0], tip[1], 1.9, 1.9), lambda x, y: 'g', 'head')  # antenna bulb
    for side in (-1, 1):  # ear bolts
        c.part(ell(cx + side * 14.0, cy + 1.6, 2.4, 3.0), lambda x, y: 'g', 'head')
    helmet = union(ell(cx, cy, 13.8, 11.8), ell(cx, cy + 4.2, 12.2, 9.0))
    c.part(helmet, lambda x, y: mint(shade(x, y, cx, cy + 1, 15, 14)), 'head')
    vx, vy, rx, ry = VISOR
    glint = ell(vx - 7.4 + dx, vy - 4.2 + dy, 2.2, 0.8)
    c.part(squircle(vx + dx, vy + dy, rx, ry), lambda x, y: 'w' if glint(x, y) else 'v', 'head')


# ---------- body ----------
def torso(c, oy=0.0):
    cy = 35.0 + oy
    mask = union(ell(CX, cy, 8.4, 6.6), ell(CX, cy - 4.0, 10.2, 3.4))
    kx, ky = CX, cy - 1.6  # glowing chest core

    def colour(x, y):
        d = math.hypot(x - kx, y - ky)
        if d < 1.0:
            return 'w'
        if d < 1.9:
            return 'e'
        if d < 2.9:
            return 'g'
        if y > cy + 4.0:
            return 'P'  # belt
        return purple(shade(x, y, CX, cy - 1, 9.5, 8.5))
    c.part(mask, colour, 'body')


def leg(c, hip, knee, foot, toe_dx=0.0):
    hx, hy = hip
    kx, ky = knee
    fx, fy = foot
    toe = ell(fx + toe_dx, fy + 0.5, 3.0, 1.9)
    mask = union(capsule(hx, hy, kx, ky, 2.5), capsule(kx, ky, fx, fy, 2.3), toe)

    def colour(x, y):
        if toe(x, y) or math.hypot(x - fx, y - fy) < 2.4:
            return 'g'  # boots
        return purple(shade(x, y, kx, ky - 2, 5, 6))
    c.part(mask, colour, 'body')


def arm(c, shoulder, elbow, hand, owner='arm'):
    sx, sy = shoulder
    ex, ey = elbow
    hx, hy = hand
    mask = union(capsule(sx, sy, ex, ey, 2.1), capsule(ex, ey, hx, hy, 1.9), ell(hx, hy, 2.7, 2.7))

    def colour(x, y):
        if (x - hx) ** 2 + (y - hy) ** 2 <= 2.8 ** 2:
            return 'o'  # round mitt
        if (x - sx) ** 2 + (y - sy) ** 2 <= 2.5 ** 2:
            return mint(shade(x, y, sx, sy, 3, 3))  # shoulder pad
        return 'P'
    c.part(mask, colour, owner)


def arms_pair(c, left, right):
    arm(c, *left)
    arm(c, *right)


def mirrored(a):
    return tuple(mirror_pt(p) for p in a)


def legs_standing(c, spread=0.0, lift=0.0):
    for side in (-1, 1):
        x = CX + side * 3.6
        leg(c, (x, 40.0 + lift), (x + side * spread, 43.0 + lift * 0.5), (x + side * spread * 0.5, 45.2),
            toe_dx=side * 0.6)


ARM_DOWN = ((15.8, 31.0), (14.6, 35.5), (14.8, 39.0))


# ---------- poses: draw back to front, return the head offset ----------
def standing(c, b=0):
    legs_standing(c)
    torso(c, b)
    down = tuple((x, y + b) for x, y in ARM_DOWN)
    arms_pair(c, down, mirrored(down))
    head(c, 0, b)
    return 0, b


def wave(c, hand):
    legs_standing(c)
    torso(c)
    arm(c, *ARM_DOWN)
    head(c, 0, 0)
    arm(c, (32.2, 30.6), (38.6, 30.0), hand)
    return 0, 0


def cheer(c, b, spread):
    legs_standing(c, spread, b * 0.5)
    torso(c, b)
    up = ((15.8, 30.6 + b), (9.4, 30.0 + b), (5.6, 21.8 + b))
    arms_pair(c, up, mirrored(up))
    head(c, 0, b)
    return 0, b


def sit(c, b):
    for side in (-1, 1):  # cross-legged: knees out, boots tucked in the middle
        leg(c, (CX + side * 3.4, 43.0), (CX + side * 12.0, 43.6), (CX + side * 4.2, 45.6), toe_dx=-side * 1.2)
    torso(c, 5)
    rest = ((15.8, 36.0 + b), (13.0, 39.5), (12.6, 42.4))
    arms_pair(c, rest, mirrored(rest))
    head(c, 0, 5 + b)
    return 0, 5 + b


def laugh(c, shake):
    legs_standing(c, 0.8)
    torso(c, 1)
    hug = ((15.8, 32.0), (14.6, 36.6), (20.2, 38.6))  # holding the belly
    arms_pair(c, hug, mirrored(hug))
    head(c, shake, 1)
    return shake, 1


def grapple(c, side):
    legs_standing(c, 0.6)
    torso(c)
    shoot = ((15.8, 30.6), (9.6, 30.4), (3.6, float(CABLE_HAND_Y)))
    hip = ((15.8, 31.0), (11.6, 35.0), (16.6, 38.4))
    if side > 0:
        shoot, hip = mirrored(shoot), mirrored(hip)
    arm(c, *hip)
    head(c, 0, 0)
    arm(c, *shoot)
    edge = 0 if side < 0 else W - 1  # cable to the sprite edge; the screen continues it
    hand_x = int(shoot[2][0])
    for x in range(min(edge, hand_x), max(edge, hand_x) + 1):
        if c.c[CABLE_HAND_Y][x] == '.':
            c.put(x, CABLE_HAND_Y, 'w', 'arm')
    return 0, 0


POSES = {
    'Relaxed': lambda c: standing(c, 0),
    'Breathe': lambda c: standing(c, 1),
    'Wave': lambda c: wave(c, (42.0, 21.6)),
    'WaveFlick': lambda c: wave(c, (43.2, 23.8)),
    'Sleep': lambda c: sit(c, 0),
    'SleepBreathe': lambda c: sit(c, 1),
    'Cheer': lambda c: cheer(c, 0, 0),
    'CheerBounce': lambda c: cheer(c, 2, 1.6),
    'Laugh': lambda c: laugh(c, 0),
    'LaughShake': lambda c: laugh(c, 1),
    'GrappleLeft': lambda c: grapple(c, -1),
    'GrappleRight': lambda c: grapple(c, 1),
}


# ---------- overlays: LED eyes on the visor, crown ----------
def eyes(c, kind, dx, dy):
    c.ox, c.oy = dx, dy
    for side in (-1, 1):
        lx = EYE_L[0] if side < 0 else 2 * CX - EYE_L[0]
        ly = EYE_L[1]
        if kind == 'Open':  # rounded LED block with a glint
            c.part(squircle(lx, ly, 2.4, 3.0),
                   lambda x, y, lx=lx: 'w' if abs(x - (lx - 0.9)) < 0.6 and abs(y - (ly - 1.4)) < 0.6 else 'e',
                   'eyes', outline=False)
        elif kind == 'Closed':  # blink: a flat line
            c.part(lambda x, y, lx=lx: abs(x - lx) <= 2.6 and abs(y - (ly + 0.6)) <= 0.6,
                   lambda x, y: 'e', 'eyes', outline=False)
        elif kind == 'Happy':  # ^ arcs
            arc, inner = ell(lx, ly + 1.6, 3.0, 3.0), ell(lx, ly + 2.4, 1.9, 2.4)
            c.part(lambda x, y, a=arc, i=inner: a(x, y) and not i(x, y) and y < ly + 1.4,
                   lambda x, y: 'e', 'eyes', outline=False)
        elif kind == 'Sleepy':  # closed ‿ arcs
            arc, inner = ell(lx, ly - 1.0, 3.0, 3.0), ell(lx, ly - 1.8, 1.9, 2.4)
            c.part(lambda x, y, a=arc, i=inner: a(x, y) and not i(x, y) and y > ly - 0.8,
                   lambda x, y: 'e', 'eyes', outline=False)
        elif kind == 'Star':  # five-point gold star
            pts = []
            for k in range(10):
                r = 3.4 if k % 2 == 0 else 1.5
                a = math.radians(-90 + 36 * k)
                pts.append((lx + r * math.cos(a), ly + 0.2 + r * math.sin(a)))
            c.part(poly(pts), lambda x, y: 'g', 'eyes', outline=False)
        elif kind == 'Heart':  # rose heart
            hx, hy = lx, ly - 0.4
            heart = union(ell(hx - 1.3, hy - 0.7, 1.5, 1.4), ell(hx + 1.3, hy - 0.7, 1.5, 1.4),
                          poly([(hx - 2.8, hy - 0.4), (hx + 2.8, hy - 0.4), (hx, hy + 2.9)]))
            c.part(heart, lambda x, y: 'o', 'eyes', outline=False)
    if kind in ('Happy', 'Star', 'Heart'):  # little LED smile
        smile, inner = ell(CX, 18.4, 2.6, 1.9), ell(CX, 17.7, 1.8, 1.4)
        c.part(lambda x, y: smile(x, y) and not inner(x, y) and y > 18.6, lambda x, y: 'e', 'eyes', outline=False)
    c.ox = c.oy = 0


def crown(c, dx, dy):
    c.ox, c.oy = dx, dy
    cx, top = HC[0] + 1.0, HC[1] - 14.6  # band resting on the helmet top, right of the antenna
    band = lambda x, y: abs(x - cx) <= 6.2 and top + 3.6 <= y <= top + 6.6
    spikes = poly([(cx - 6.2, top + 4.0), (cx - 6.0, top + 0.2), (cx - 3.1, top + 3.0), (cx, top - 0.2),
                   (cx + 3.1, top + 3.0), (cx + 6.0, top + 0.2), (cx + 6.2, top + 4.0)])
    c.part(union(band, spikes), lambda x, y: 'g', 'crown')
    c.ox = c.oy = 0


def render(pose, kind=None, with_crown=False):
    c = Canvas()
    dx, dy = POSES[pose](c)
    if kind:
        eyes(c, kind, dx, dy)
    if with_crown:
        crown(c, dx, dy)
    return c, (dx, dy)


def overlay(draw):
    c = Canvas()
    draw(c)
    pts = [(x, y) for y in range(H) for x in range(W) if c.c[y][x] != '.']
    return pts, c


def build():
    bodies, offsets = [], []
    for name in POSE_ORDER:
        c, off = render(name)
        bodies.append(c)
        offsets.append(off)
    eye_canvases = [overlay(lambda c, k=k: eyes(c, k, 0, 0)) for k in EYE_ORDER]
    crown_canvas = overlay(lambda c: crown(c, 0, 0))
    ex = [p for pts, _ in eye_canvases for p in pts]
    ex0, ey0 = min(x for x, _ in ex), min(y for _, y in ex)
    ew, eh = max(x for x, _ in ex) - ex0 + 1, max(y for _, y in ex) - ey0 + 1
    cpts = crown_canvas[0]
    cx0, cy0 = min(x for x, _ in cpts), min(y for _, y in cpts)
    cw, ch = max(x for x, _ in cpts) - cx0 + 1, max(y for _, y in cpts) - cy0 + 1
    eye_rows = [[''.join(cv.c[ey0 + y][ex0:ex0 + ew]) for y in range(eh)] for _, cv in eye_canvases]
    crown_rows = [''.join(crown_canvas[1].c[cy0 + y][cx0:cx0 + cw]) for y in range(ch)]

    def stamp(canvas_rows, rows, x0, y0):
        out = [list(r) for r in canvas_rows]
        for y, row in enumerate(rows):
            for x, chr_ in enumerate(row):
                if chr_ != '.':
                    assert 0 <= y0 + y < H and 0 <= x0 + x < W, 'overlay outside the sprite'
                    out[y0 + y][x0 + x] = chr_
        return [''.join(r) for r in out]

    # Runtime composition must equal a direct render, and overlays may only cover head/background;
    # the LED eyes must stay on the visor.
    for p, name in enumerate(POSE_ORDER):
        dx, dy = offsets[p]
        base = bodies[p]
        for e, kind in enumerate(EYE_ORDER):
            for with_crown in (False, True):
                composed = stamp(base.rows(), eye_rows[e], ex0 + dx, ey0 + dy)
                if with_crown:
                    composed = stamp(composed, crown_rows, cx0 + dx, cy0 + dy)
                direct, _ = render(name, kind, with_crown)
                assert composed == direct.rows(), f'composition mismatch {name}/{kind}/{with_crown}'
            for y, row in enumerate(eye_rows[e]):
                for x, chr_ in enumerate(row):
                    if chr_ != '.':
                        assert base.c[ey0 + dy + y][ex0 + dx + x] == 'v', f'LED off the visor in {name}/{kind}'
        for y, row in enumerate(crown_rows):
            for x, chr_ in enumerate(row):
                if chr_ != '.':
                    assert base.owner[cy0 + dy + y][cx0 + dx + x] in (None, 'head'), f'crown covers body in {name}'
    head_top = min(y for y in range(H) for x in range(W) if bodies[0].owner[y][x] == 'head')
    head_bottom = max(y for y in range(H) for x in range(W) if bodies[0].owner[y][x] == 'head')
    return {
        'bodies': [b.rows() for b in bodies], 'offsets': offsets, 'eyes': eye_rows, 'crown': crown_rows,
        'eye_rect': (ex0, ey0, ew, eh), 'crown_rect': (cx0, cy0, cw, ch), 'head_top': head_top,
        'head_bottom': head_bottom,
    }


def rgb565(rgb):
    r, g, b = rgb
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def emit(data):
    nl = chr(10)
    ex, ey, ew, eh = data['eye_rect']
    cx, cy, cw, ch = data['crown_rect']
    out = [
        '// Generated by tools/pet_art/generate_pet.py - do not edit by hand.',
        '// 48x48 VOLT robot mascot; constant rows live in flash.',
        '#pragma once',
        '#include <stdint.h>',
        '#include "SpriteInk.h"',
        '',
        'namespace deskos {',
        'namespace petart {',
        f'constexpr uint8_t SIZE={W},POSES={len(POSE_ORDER)},EYES={len(EYE_ORDER)};',
        f'constexpr uint8_t HEAD_TOP={data["head_top"]},HEAD_BOTTOM={data["head_bottom"]},CABLE_HAND_Y={CABLE_HAND_Y};',
        f'constexpr uint8_t EYE_X={ex},EYE_Y={ey},EYE_W={ew},EYE_H={eh};',
        f'constexpr uint8_t CROWN_X={cx},CROWN_Y={cy},CROWN_W={cw},CROWN_H={ch};',
        f'constexpr uint8_t INK_COUNT={len(PALETTE)};',
        'const SpriteInk INKS[INK_COUNT]={',
    ]
    for i, (key, rgb, note) in enumerate(PALETTE):
        sep = ',' if i < len(PALETTE) - 1 else ''
        out.append(f"  {{'{key}',0x{rgb565(rgb):04X}}}{sep}  // {note} #{rgb[0]:02X}{rgb[1]:02X}{rgb[2]:02X}")
    out.append('};')
    out.append('// Head offset of each pose (dx, dy); overlays move with the head.')
    offs = ','.join(f'{{{dx},{dy}}}' for dx, dy in data['offsets'])
    out.append(f'const int8_t HEAD_OFFSET[POSES][2]={{{offs}}};')
    out.append('const char FRAMES[POSES][SIZE][SIZE+1]={')
    for p, name in enumerate(POSE_ORDER):
        out.append(f'  {{ // {name}')
        rows = data['bodies'][p]
        out += [f'    "{r}"' + (',' if i < H - 1 else '') for i, r in enumerate(rows)]
        out.append('  }' + (',' if p < len(POSE_ORDER) - 1 else ''))
    out.append('};')
    out.append("// LED-eye overlays ('.' = keep the frame), drawn at (EYE_X,EYE_Y) + head offset.")
    out.append('const char LENSES[EYES][EYE_H][EYE_W+1]={')
    for e, kind in enumerate(EYE_ORDER):
        out.append(f'  {{ // {kind}')
        rows = data['eyes'][e]
        out += [f'    "{r}"' + (',' if i < eh - 1 else '') for i, r in enumerate(rows)]
        out.append('  }' + (',' if e < len(EYE_ORDER) - 1 else ''))
    out.append('};')
    out.append('const char CROWN[CROWN_H][CROWN_W+1]={')
    out += [f'  "{r}"' + (',' if i < ch - 1 else '') for i, r in enumerate(data['crown'])]
    out.append('};')
    lo_x = min(dx for dx, _ in data['offsets'])
    hi_x = max(dx for dx, _ in data['offsets'])
    hi_y = max(dy for _, dy in data['offsets'])
    out.append(f'static_assert(EYE_X+({lo_x})>=0 && EYE_X+({hi_x})+EYE_W<=SIZE && EYE_Y+{hi_y}+EYE_H<=SIZE,"Eyes outside sprite");')
    out.append(f'static_assert(CROWN_X+({lo_x})>=0 && CROWN_X+({hi_x})+CROWN_W<=SIZE && CROWN_Y+{hi_y}+CROWN_H<=SIZE,'
               '"Crown outside sprite");')
    out.append('}  // namespace petart')
    out.append('}  // namespace deskos')
    return nl.join(out) + nl


def preview(path):
    colours = {'.': (0x17, 0x13, 0x29)}
    colours.update({k: rgb for k, rgb, _ in PALETTE})
    samples = [(p, 'Open', False) for p in POSE_ORDER] + [('Relaxed', k, k == 'Star') for k in EYE_ORDER[1:]]
    samples[2] = ('Wave', 'Happy', False)
    samples[4] = ('Sleep', 'Sleepy', False)
    samples[8] = ('Laugh', 'Happy', False)
    scale, pad, cols = 4, 6, 6
    rows_n = (len(samples) + cols - 1) // cols
    w = pad + cols * (W * scale + pad)
    h = pad + rows_n * (H * scale + pad) + H * 2 + pad
    px = bytearray(bytes(colours['.']) * (w * h))

    def put(x, y, s, col):
        for yy in range(y, y + s):
            o = (yy * w + x) * 3
            px[o:o + 3 * s] = bytes(col) * s
    for i, (pose, kind, cr) in enumerate(samples):
        c, _ = render(pose, kind, cr)
        ox, oy = pad + (i % cols) * (W * scale + pad), pad + (i // cols) * (H * scale + pad)
        for y, row in enumerate(c.rows()):
            for x, chr_ in enumerate(row):
                put(ox + x * scale, oy + y * scale, scale, colours[chr_])
                if i < 9:
                    put(pad + i * (W * 2 + pad) + x * 2, pad + rows_n * (H * scale + pad) + y * 2, 2, colours[chr_])
    raw = b''.join(b'\0' + px[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(t, d):
        return struct.pack('!I', len(d)) + t + d + struct.pack('!I', zlib.crc32(t + d))
    Path(path).write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!2I5B', w, h, 8, 2, 0, 0, 0)) +
                           chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
    print(f'Preview -> {path}')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--check', action='store_true', help='exit 1 if the header differs from the generator')
    parser.add_argument('--preview', help='write a quick PNG of every frame instead of the header')
    args = parser.parse_args()
    if args.preview:
        preview(args.preview)
        return 0
    text = emit(build())
    if args.check:
        current = OUTPUT.read_text(encoding='utf-8') if OUTPUT.exists() else ''
        if current.replace('\r\n', '\n') != text:
            print(f'{OUTPUT} is stale: run tools/pet_art/generate_pet.py')
            return 1
        print('PASS: PetSpriteData.h matches the generator (composition and overlay checks OK)')
        return 0
    OUTPUT.write_text(text, encoding='utf-8', newline='')
    print(f'Wrote {OUTPUT}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
