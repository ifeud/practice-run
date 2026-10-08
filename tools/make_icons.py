#!/usr/bin/env python3
"""
Generates the mod's images.

  resources/practiceIcon.png        - the purple practice-run button (inactive)
  resources/practiceIconActive.png  - the same button with a red "no entry"
                                      mark, shown while a tracked run is active
  resources/settingsIcon.png        - the small purple gear button in the corner
                                      of the pause menu that opens the
                                      performance settings popup
  logo.png                          - the mod's icon in the Geode mods list

All of them are drawn the same way as Geometry Dash's own round pause-menu
buttons, whose layout was measured pixel by pixel off a screenshot:

    outer white ring        1.000 -> 0.920 of the radius R
    black ring              0.920 -> 0.833
    coloured circle         0.833                (diagonal gradient)
    diamond                 half width 0.395 R, half height 0.720 R
      black outline         0.042 R thick
      white stroke          0.082 R thick
      fill                  pale top half, vivid bottom half

and the red "no entry" mark of the game's own practice button:

    ring radius 0.663 R, stroke 0.087 R, white outline 0.065 R

Run it with:  python3 tools/make_icons.py
"""

import math
import os

from PIL import Image, ImageDraw

# --- geometry, as fractions of the outer radius R ---
RING_FRAC = 0.918         # outer radius / (half the image size)
BLACK_RING_IN = 0.920     # black ring outer radius / R
CIRCLE = 0.833            # coloured circle radius / R
DIA_HALF_W = 0.395        # diamond half width / R
DIA_HALF_H = 0.720        # diamond half height / R
DIA_OUTLINE = 0.042       # black outline thickness / R (perpendicular)
DIA_STROKE = 0.082        # white stroke thickness / R (perpendicular)

NO_RING = 0.663           # red mark: ring radius / R
NO_STROKE = 0.087         # red mark: ring and bar thickness / R
NO_OUTLINE = 0.065        # red mark: white outline thickness / R

GEAR_TEETH = 8            # the gear in the settings icon
GEAR_OUTER = 0.50         # gear body radius / R (teeth stick out of it)
GEAR_HOLE = 0.26          # gear centre hole radius / R
GEAR_TOOTH_W = 0.36       # tooth width, as a fraction of the space per tooth
GEAR_TOOTH_L = 0.85       # tooth length, as a fraction of the gear radius

# --- colours ---
WHITE = (255, 255, 255, 255)
BLACK = (0, 0, 0, 255)
CIRCLE_LIGHT = (182, 124, 234, 255)   # gradient: light purple, top left ...
CIRCLE_DARK = (104, 52, 156, 255)     # ... to dark purple, bottom right
DIA_TOP = (222, 196, 252, 255)        # pale lavender: the diamond's top half
DIA_BOTTOM = (152, 88, 232, 255)      # vivid purple: its bottom half
RED = (230, 36, 36, 255)              # the "no entry" mark

SS = 4  # supersampling factor


def diagonal_gradient(size, light, dark):
    """A diagonal (top-left to bottom-right) gradient covering `size` x `size`."""
    img = Image.new("RGBA", (size, size))
    p = img.load()
    for y in range(size):
        for x in range(size):
            t = (x + y) / (2 * (size - 1))
            p[x, y] = tuple(
                round(light[i] + (dark[i] - light[i]) * t) for i in range(4)
            )
    return img


def circle_mask(size, radius, centre):
    mask = Image.new("L", (size, size), 0)
    c = centre
    ImageDraw.Draw(mask).ellipse(
        [c - radius, c - radius, c + radius, c + radius], fill=255
    )
    return mask


def diamond_points(cx, cy, half_w, half_h):
    return [(cx, cy - half_h), (cx + half_w, cy), (cx, cy + half_h), (cx - half_w, cy)]


def inset(half_w, half_h, t):
    """Shrink a diamond by `t` perpendicular to its edges."""
    diag = (half_w * half_w + half_h * half_h) ** 0.5
    return half_w - t * diag / half_h, half_h - t * diag / half_w


def rotated_rect(cx, cy, w, h, angle):
    """The four corners of a rectangle centred on (cx, cy), rotated by `angle`."""
    ca, sa = math.cos(angle), math.sin(angle)
    pts = [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)]
    return [(cx + x * ca - y * sa, cy + x * sa + y * ca) for x, y in pts]


def draw_round_button(size, ring_frac=RING_FRAC, diamond=True):
    """The purple round button. With `diamond=False` only the ring and the
    coloured circle are drawn, which is the base for the settings icon."""
    S = size * SS
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = S / 2
    R = (S / 2) * ring_frac

    # 1) white ring, 2) black ring, 3) the coloured circle
    d.ellipse([c - R, c - R, c + R, c + R], fill=WHITE)
    r_black = BLACK_RING_IN * R
    d.ellipse([c - r_black, c - r_black, c + r_black, c + r_black], fill=BLACK)
    r_circle = CIRCLE * R
    img.paste(
        diagonal_gradient(S, CIRCLE_LIGHT, CIRCLE_DARK),
        (0, 0),
        circle_mask(S, r_circle, c),
    )

    if not diamond:
        return img.resize((size, size), Image.LANCZOS)

    # 4) the diamond, from the outside in
    hw, hh = DIA_HALF_W * R, DIA_HALF_H * R
    d.polygon(diamond_points(c, c, hw, hh), fill=BLACK)

    hw, hh = inset(hw, hh, DIA_OUTLINE * R)
    d.polygon(diamond_points(c, c, hw, hh), fill=WHITE)

    hw, hh = inset(hw, hh, DIA_STROKE * R)
    fill_mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(fill_mask).polygon(diamond_points(c, c, hw, hh), fill=255)

    # the fill is two flat halves - pale on top, vivid underneath
    fill = Image.new("RGBA", (S, S), DIA_BOTTOM)
    ImageDraw.Draw(fill).rectangle([0, 0, S, c - 1], fill=DIA_TOP)
    img.paste(fill, (0, 0), fill_mask)

    return img.resize((size, size), Image.LANCZOS)


def draw_no_mark(size, ring_frac=RING_FRAC):
    """The red 'no entry' mark (ring + diagonal bar) with a white outline."""
    S = size * SS
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = S / 2
    R = (S / 2) * ring_frac
    r_ring = NO_RING * R
    bar_len = r_ring * 2 * 0.98

    # the white outline is drawn first, a bit thicker, so the red sits on top
    # of it and only the edge sticks out - exactly how the game's button does it
    for thickness, color in (
        (NO_STROKE + 2 * NO_OUTLINE, WHITE),
        (NO_STROKE, RED),
    ):
        w = thickness * R
        d.ellipse([c - r_ring, c - r_ring, c + r_ring, c + r_ring],
                  outline=color, width=max(1, round(w)))
        # the bar goes from the bottom left to the top right
        d.polygon(rotated_rect(c, c, bar_len, w, -math.pi / 4), fill=color)

    return img.resize((size, size), Image.LANCZOS)


def draw_gear_mark(size, ring_frac=RING_FRAC):
    """A white gear, used in the middle of the settings button."""
    S = size * SS
    img = Image.new("L", (S, S), 0)   # a mask: white = "paint here"
    d = ImageDraw.Draw(img)
    c = S / 2
    R = (S / 2) * ring_frac
    r_gear = GEAR_OUTER * R
    r_hole = GEAR_HOLE * R
    tooth_len = GEAR_TOOTH_L * r_gear
    tooth_w = 2 * math.pi * r_gear / GEAR_TEETH * GEAR_TOOTH_W

    d.ellipse([c - r_gear, c - r_gear, c + r_gear, c + r_gear], fill=255)
    for i in range(GEAR_TEETH):
        angle = 2 * math.pi * i / GEAR_TEETH
        # the tooth starts inside the body and sticks outwards, so the two
        # always overlap and never leave a gap
        dist = r_gear + tooth_len / 2 - 2
        tx = c + math.cos(angle) * dist
        ty = c + math.sin(angle) * dist
        d.polygon(rotated_rect(tx, ty, tooth_w, tooth_len, angle), fill=255)
    d.ellipse([c - r_hole, c - r_hole, c + r_hole, c + r_hole], fill=0)

    return img.resize((size, size), Image.LANCZOS)


def draw_practice_icon(size, active=False):
    icon = draw_round_button(size)
    if active:
        icon = Image.alpha_composite(icon, draw_no_mark(size))
    return icon


def draw_settings_icon(size):
    # no diamond here: a plain purple circle with a white gear on it, so it is
    # obvious at a glance that this button is not the practice button
    icon = draw_round_button(size, diamond=False)
    gear = draw_gear_mark(size)
    icon.paste(WHITE, (0, 0), gear)
    return icon


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    resources = os.path.join(root, "resources")
    os.makedirs(resources, exist_ok=True)

    # 264 px master -> 264/-uhd (66 units), 132/-hd and 66 px plain. The game
    # shows a round button of exactly 66 x 66 design units - the same size as
    # its own practice / play / retry buttons.
    draw_practice_icon(264, active=False).save(os.path.join(resources, "practiceIcon.png"))
    draw_practice_icon(264, active=True).save(os.path.join(resources, "practiceIconActive.png"))

    # 176 px master -> 44 units, so the visible circle is ~40 units: a slightly
    # smaller "utility" button for the corner of the pause menu.
    draw_settings_icon(176).save(os.path.join(resources, "settingsIcon.png"))

    # The mods-list logo: same drawing, nearly filling the square, still with a
    # fully transparent background (no white square behind it).
    draw_round_button(320, 0.98).save(os.path.join(root, "logo.png"))

    print("wrote resources/practiceIcon.png, resources/practiceIconActive.png, "
          "resources/settingsIcon.png and logo.png")


if __name__ == "__main__":
    main()
