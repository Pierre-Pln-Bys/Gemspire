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



def card(cx, cy, w, h, gem, points, angle):
    global img, d
    layer = Image.new("RGBA", (int(w * 1.6), int(h * 1.6)), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    ox, oy = w * 0.3, h * 0.3
    body = tuple(int(255 - (255 - c) * 0.3) for c in colors[gem])
    ld.rounded_rectangle([ox, oy, ox + w, oy + h], radius=w * 0.08, fill=body, outline=(20, 20, 20), width=int(3 * S))
    ld.rounded_rectangle([ox, oy, ox + w, oy + h * 0.24], radius=w * 0.08, fill=colors[gem])
    ld.rectangle([ox, oy + h * 0.12, ox + w, oy + h * 0.24], fill=colors[gem])
    f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", int(h * 0.17))
    ld.text((ox + w * 0.1, oy + h * 0.02), str(points), font=f, fill=(255, 255, 255) if gem else (40, 40, 40))
    layer = layer.rotate(angle, resample=Image.BICUBIC, expand=False)
    sh = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    sh.paste((0, 0, 0, 110), mask=layer.split()[3])
    sh = sh.filter(ImageFilter.GaussianBlur(8 * S))
    px, py = int(cx - layer.size[0] / 2), int(cy - layer.size[1] / 2)
    img.alpha_composite(sh, (px + int(8 * S), py + int(12 * S)))
    img.alpha_composite(layer, (px, py))
    d = ImageDraw.Draw(img)

cw, ch = W * 0.1, W * 0.14
card(W * 0.05, H * 0.2, cw, ch, 1, 2, 18)
card(W * 0.12, H * 0.85, cw, ch, 3, 4, -12)
card(W * 0.95, H * 0.78, cw, ch, 2, 3, -20)
card(W * 0.9, H * 0.12, cw, ch, 4, 1, 10)
r = H * 0.075
for gem, (fx, fy) in enumerate([(0.2, 0.08), (0.03, 0.55), (0.28, 0.93), (0.76, 0.93), (0.98, 0.45)]):
    token(gem, W * fx, H * fy, r, colors[gem])
token(5, W * 0.8, H * 0.06, r * 1.2, gold)
# centre assombri pour l'interface de Discord
shade = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ImageDraw.Draw(shade).ellipse([W * 0.2, H * 0.12, W * 0.8, H * 0.88], fill=(0, 0, 0, 90))
img.alpha_composite(shade.filter(ImageFilter.GaussianBlur(60 * S)))
img.convert("RGB").resize((1024, 576), Image.LANCZOS).save(sys.argv[1])
