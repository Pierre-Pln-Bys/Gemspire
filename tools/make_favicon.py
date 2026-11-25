"""Génère server/favicon.hpp (images du site) à partir de assets/.

Usage : python3 tools/make_favicon.py   (demande Pillow)
- favicon.ico : 16, 32 et 48 px (onglets, favoris, résultats de recherche),
  depuis assets/logo.png ;
- apple-touch-icon.png : 180 px, fond plein (iPhone, iPad, raccourcis Android) ;
- og-image.jpg : 1024 x 576, aperçu des liens partagés (Discord, réseaux
  sociaux, moteurs de recherche), depuis assets/invite-cover.png.
"""
import io
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
FELT = (15, 61, 40, 255)  # même vert que le site (--felt)


def png_bytes(image):
    out = io.BytesIO()
    image.save(out, format="PNG", optimize=True)
    return out.getvalue()


def c_array(name, data):
    lines = [f"inline const unsigned char {name}[] = {{"]
    for start in range(0, len(data), 20):
        chunk = data[start:start + 20]
        lines.append("    " + ", ".join(str(b) for b in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def main():
    logo = Image.open(ROOT / "assets" / "logo.png").convert("RGBA")

    ico = io.BytesIO()
    logo.save(ico, format="ICO", sizes=[(16, 16), (32, 32), (48, 48)])

    # Les coins transparents deviendraient noirs sur iOS : fond vert plein
    touch = Image.new("RGBA", logo.size, FELT)
    touch.alpha_composite(logo)
    touch = touch.convert("RGB").resize((180, 180), Image.LANCZOS)

    cover = Image.open(ROOT / "assets" / "invite-cover.png").convert("RGB")
    og = io.BytesIO()
    cover.save(og, format="JPEG", quality=85, optimize=True, progressive=True)

    header = "\n\n".join([
        "// Fichier généré par tools/make_favicon.py à partir de assets/ : ne pas modifier à la main.",
        "#pragma once",
        c_array("FAVICON_ICO", ico.getvalue()),
        c_array("APPLE_TOUCH_ICON_PNG", png_bytes(touch)),
        c_array("OG_IMAGE_JPG", og.getvalue()),
    ]) + "\n"
    (ROOT / "server" / "favicon.hpp").write_text(header)


if __name__ == "__main__":
    main()
