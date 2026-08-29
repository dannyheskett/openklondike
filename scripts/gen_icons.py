#!/usr/bin/env python3
"""Generate every launcher / store icon from one vector description.

The icons are the game itself in miniature: a blue card back behind a white card
face carrying the heart pip that render.c draws, on the felt green of the table.
Keeping them generated rather than hand-drawn means the palette can never drift
from src/render.c, and every size is produced from the same geometry.

Outputs (run from the repo root, needs Pillow):
    android/res/mipmap-*/ic_launcher.png            legacy square launcher icon
    android/res/mipmap-*/ic_launcher_foreground.png adaptive-icon foreground
    android/play-assets/icon-512.png                Play store listing icon
    android/play-assets/feature-graphic-1024x500.png Play store feature graphic
    ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png  iOS app icon
    ios/app-store-assets/icon-1024.png              App Store listing icon

    scripts/gen_icons.py
"""
import math
import os

from PIL import Image, ImageDraw

# Palette, copied from the constants at the top of src/render.c.
FELT = (12, 92, 52, 255)
FELT_DARK = (10, 76, 44, 255)
CARD_FACE = (248, 248, 242, 255)
CARD_EDGE = (40, 40, 40, 255)
CARD_BACK = (36, 72, 156, 255)
CARD_BACK2 = (80, 130, 220, 255)
RED_PIP = (200, 30, 40, 255)

SS = 4  # supersample factor; every shape is drawn large and downscaled

# Card proportions, matching CARD_W x CARD_H in src/render.h.
CARD_ASPECT = 112 / 80


def heart(cx, cy, size, squash=0.86, segments=90):
    """The heart outline render.c draws, as a list of points.

    x = 16 sin^3 t,  y = 13 cos t - 5 cos 2t - 2 cos 3t - cos 4t
    """
    raw = []
    for i in range(segments):
        t = i / segments * 2 * math.pi
        x = 16 * math.sin(t) ** 3
        y = -(13 * math.cos(t) - 5 * math.cos(2 * t)
              - 2 * math.cos(3 * t) - math.cos(4 * t))
        raw.append((x, y))
    xs = [p[0] for p in raw]
    ys = [p[1] for p in raw]
    scale = size / (max(ys) - min(ys))
    mx = (min(xs) + max(xs)) / 2
    my = (min(ys) + max(ys)) / 2
    return [(cx + (x - mx) * scale * squash, cy + (y - my) * scale) for x, y in raw]


def card_back(w, h):
    """A blue card back with the bounded plaid panel, as an RGBA image."""
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    r = int(w * 0.12)
    edge = max(1, int(w * 0.03))
    d.rounded_rectangle([0, 0, w - 1, h - 1], r, fill=CARD_BACK, outline=CARD_EDGE,
                        width=edge)
    m = int(w * 0.12)
    lw = max(1, int(w * 0.012))
    d.rectangle([m, m, w - m, h - m], outline=CARD_BACK2, width=lw)
    step = int(w * 0.1)
    for gx in range(m + step, w - m, step):
        d.line([gx, m, gx, h - m], fill=CARD_BACK2, width=lw)
    for gy in range(m + step, h - m, step):
        d.line([m, gy, w - m, gy], fill=CARD_BACK2, width=lw)
    return img


def card_face(w, h):
    """A white card face carrying a large centred heart pip."""
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    r = int(w * 0.12)
    edge = max(1, int(w * 0.03))
    d.rounded_rectangle([0, 0, w - 1, h - 1], r, fill=CARD_FACE, outline=CARD_EDGE,
                        width=edge)
    d.polygon(heart(w / 2, h / 2, h * 0.46), fill=RED_PIP)
    return img


def compose(size, background, content_scale):
    """The icon at `size` px. `background` is None for the adaptive foreground.

    `content_scale` is the fraction of the canvas the card pair spans, so the
    adaptive foreground can stay inside its 66% safe zone while the legacy
    square icon fills more of its tile.
    """
    n = size * SS
    img = Image.new("RGBA", (n, n), background if background else (0, 0, 0, 0))

    card_h = int(n * content_scale)
    card_w = int(card_h / CARD_ASPECT)
    cx, cy = n // 2, n // 2

    # The back card sits behind and to the left, tilted the other way, so the
    # pair reads as a fanned hand rather than one card at every size.
    back = card_back(card_w, card_h).rotate(16, resample=Image.BICUBIC, expand=True)
    img.alpha_composite(back, (cx - back.width // 2 - int(card_w * 0.30),
                               cy - back.height // 2))
    face = card_face(card_w, card_h).rotate(-8, resample=Image.BICUBIC, expand=True)
    img.alpha_composite(face, (cx - face.width // 2 + int(card_w * 0.18),
                               cy - face.height // 2))

    return img.resize((size, size), Image.LANCZOS)


def feature_graphic(w, h):
    """Play's 1024x500 feature graphic: the icon art on a felt gradient."""
    img = Image.new("RGBA", (w * 2, h * 2), FELT)
    d = ImageDraw.Draw(img)
    for y in range(h * 2):  # subtle vertical shade, dark at the bottom
        t = y / (h * 2)
        d.line([0, y, w * 2, y],
               fill=tuple(int(FELT[i] + (FELT_DARK[i] - FELT[i]) * t) for i in range(3)))
    art = compose(h * 2, None, 0.62)
    img.alpha_composite(art, ((w * 2 - art.width) // 2, 0))
    return img.resize((w, h), Image.LANCZOS)


def save(img, path, opaque=False):
    """Write `img`, flattening away the alpha channel when `opaque` is set.

    Apple rejects an app icon that has an alpha channel outright -- it masks the
    corners itself -- so the iOS icons must be flat RGB. The Android adaptive
    foreground is the opposite case and must keep its transparency.
    """
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if opaque:
        flat = Image.new("RGB", img.size, FELT[:3])
        flat.paste(img, mask=img.split()[3])
        img = flat
    img.save(path)
    print("gen_icons: wrote %s (%dx%d %s)" % (path, img.width, img.height, img.mode))


def main():
    # Legacy square launcher icon and the adaptive foreground, per density. The
    # adaptive foreground canvas is 108dp to the legacy 48dp, and its content
    # must stay inside the central 72dp, hence the smaller content scale.
    for suffix, legacy in (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96),
                           ("xxhdpi", 144), ("xxxhdpi", 192)):
        d = "android/res/mipmap-%s" % suffix
        save(compose(legacy, FELT, 0.78), "%s/ic_launcher.png" % d)
        save(compose(legacy * 108 // 48, None, 0.52),
             "%s/ic_launcher_foreground.png" % d)

    save(compose(512, FELT, 0.72), "android/play-assets/icon-512.png")
    save(feature_graphic(1024, 500), "android/play-assets/feature-graphic-1024x500.png",
         opaque=True)

    ios_icon = compose(1024, FELT, 0.72)
    save(ios_icon, "ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png", opaque=True)
    save(ios_icon, "ios/app-store-assets/icon-1024.png", opaque=True)


if __name__ == "__main__":
    main()
