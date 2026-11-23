import math
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

S = 3
W, H = 1024 * S, 576 * S
img = Image.new("RGBA", (W, H), (0, 0, 0, 255))
d = ImageDraw.Draw(img)

# Tapis vert, plus clair au centre
for i in range(240, 0, -1):
    t = i / 240
    c = (int(10 + 24 * (1 - t)), int(44 + 58 * (1 - t)), int(30 + 30 * (1 - t)), 255)
    rx, ry = W * 0.85 * t, H * 1.1 * t
    d.ellipse([W * 0.5 - rx, H * 0.5 - ry, W * 0.5 + rx, H * 0.5 + ry], fill=c)

colors = [(242, 242, 237), (41, 97, 209), (26, 148, 77), (217, 41, 46), (31, 31, 33)]
gold = (245, 194, 31)


def poly(cx, cy, sides, radius, rot):
    return [(cx + radius * math.cos(math.radians(rot + 360 * k / sides)),
             cy + radius * math.sin(math.radians(rot + 360 * k / sides))) for k in range(sides)]


def symbol(gem, cx, cy, size, fill):
    if gem == 0:
        d.ellipse([cx - size * 0.8, cy - size * 0.8, cx + size * 0.8, cy + size * 0.8], fill=fill)
    elif gem == 1:
        d.polygon(poly(cx, cy, 4, size, 0), fill=fill)
    elif gem == 2:
        d.polygon(poly(cx, cy + size * 0.15, 3, size, -90), fill=fill)
    elif gem == 3:
        d.polygon(poly(cx, cy, 4, size, 45), fill=fill)
    elif gem == 4:
        d.polygon(poly(cx, cy, 6, size, 0), fill=fill)
    else:
        d.polygon(poly(cx, cy, 3, size, -90), fill=fill)
        d.polygon(poly(cx, cy, 3, size, 90), fill=fill)


def token(gem, cx, cy, radius, color):
    global img, d
    sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(sh).ellipse([cx - radius + 8 * S, cy - radius + 14 * S, cx + radius + 8 * S, cy + radius + 14 * S],
                               fill=(0, 0, 0, 120))
    img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(10 * S)))
    d = ImageDraw.Draw(img)
    dark = tuple(int(c * 0.75) for c in color)
    d.ellipse([cx - radius, cy - radius, cx + radius, cy + radius], fill=dark)
    r2 = radius * 0.82
    d.ellipse([cx - r2, cy - r2, cx + r2, cy + r2], fill=color)
    dash = (170, 170, 170) if gem == 0 else (255, 255, 255)
    for k in range(12):
        d.arc([cx - radius * 0.94, cy - radius * 0.94, cx + radius * 0.94, cy + radius * 0.94], k * 30, k * 30 + 15,
              fill=dash, width=int(radius * 0.08))
    on = (40, 40, 40) if gem in (0, 5) else (255, 255, 255)
    symbol(gem, cx, cy, radius * 0.42, on)


# Jetons à gauche, en cercle autour de l'or
cx, cy, ring = W * 0.23, H * 0.5, H * 0.28
for gem in range(5):
    a = math.radians(-90 + 72 * gem)
    token(gem, cx + ring * math.cos(a), cy + ring * math.sin(a), H * 0.12, colors[gem])
token(5, cx, cy, H * 0.155, gold)

# Texte à droite
bold = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
regular = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
x = W * 0.49
title = ImageFont.truetype(bold, int(H * 0.145))
sub = ImageFont.truetype(regular, int(H * 0.054))
small = ImageFont.truetype(bold, int(H * 0.048))
# ombre du titre
shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ImageDraw.Draw(shadow).text((x + 6 * S, H * 0.28 + 8 * S), "Gemspire", font=title, fill=(0, 0, 0, 150))
img.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(6 * S)))
d = ImageDraw.Draw(img)
d.text((x, H * 0.28), "Gemspire", font=title, fill=gold)
d.text((x + 4 * S, H * 0.52), "Gems, nobles and prestige", font=sub, fill=(235, 235, 228))
d.text((x + 4 * S, H * 0.62), "2 to 4 players, right in your call", font=sub, fill=(190, 214, 198))
# pastille « Rejoins la partie »
label = "PLAY NOW"
bx, by = x + 4 * S, H * 0.75
tw = d.textlength(label, font=small)
d.rounded_rectangle([bx, by, bx + tw + 48 * S, by + H * 0.1], radius=H * 0.05, fill=gold)
d.text((bx + 24 * S, by + H * 0.05), label, font=small, fill=(30, 40, 30), anchor="lm")

img.convert("RGB").resize((1024, 576), Image.LANCZOS).save(sys.argv[1])
