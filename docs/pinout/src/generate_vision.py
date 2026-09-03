"""Erzeugt docs/pinout/massarbeit-vision-pinout.png aus dem offiziellen
LilyGO-Pinout-Bild (t-display-s3-official.jpg, in diesem Ordner).

Pixelkoordinaten der Pads wurden durch Analyse des Original-Bilds ermittelt
(Header-Pin "1"/"2"/"3V"/"G" auf dem linken 2x12-Header). Bei einer neuen
Bild-Revision von LilyGO muessen sie neu bestimmt werden.

Ausfuehren aus docs/pinout/src/:  python generate_vision.py
"""

from PIL import Image, ImageDraw, ImageFont

FONT_DIR = "C:/Windows/Fonts"

src = Image.open("t-display-s3-official.jpg").convert("RGB")

box = (500, 270, 945, 800)
scale = 1.6
crop = src.crop(box)
crop = crop.resize((int(crop.width * scale), int(crop.height * scale)), Image.LANCZOS)


def tx(pt):
    x, y = pt
    return ((x - box[0]) * scale, (y - box[1]) * scale)


# Badge-Nummer, Pad-Koordinate im Original, Farbe
pins = [
    ("1", (588, 357), "#dc2626"),  # 3V  -> HX711 VCC
    ("2", (588, 393), "#d97706"),  # GPIO1 -> HX711 DOUT
    ("3", (588, 429), "#d97706"),  # GPIO2 -> HX711 SCK
    ("4", (588, 717), "#1f2933"),  # G   -> HX711 GND
]

d = ImageDraw.Draw(crop)
font_badge = ImageFont.truetype(f"{FONT_DIR}/arialbd.ttf", 26)
for num, pt, color in pins:
    cx, cy = tx(pt)
    r = 22
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=color, width=6)
    bx, by = cx - 70, cy
    br = 20
    d.ellipse([bx - br, by - br, bx + br, by + br], fill=color, outline="#ffffff", width=3)
    tb = d.textbbox((0, 0), num, font=font_badge)
    tw, th = tb[2] - tb[0], tb[3] - tb[1]
    d.text((bx - tw / 2 - tb[0], by - th / 2 - tb[1]), num, font=font_badge, fill="#ffffff")
    d.line([bx + br, by, cx - r, cy], fill=color, width=4)

# ---- Gesamtcanvas: Titel + Bild + Legende + Fussnote ----
W = max(crop.width + 80, 760)
title_h, legend_h, footer_h = 100, 200, 60
H = title_h + crop.height + legend_h + footer_h

canvas = Image.new("RGB", (W, H), "#ffffff")
cd = ImageDraw.Draw(canvas)
font_title = ImageFont.truetype(f"{FONT_DIR}/arialbd.ttf", 30)
font_sub = ImageFont.truetype(f"{FONT_DIR}/arial.ttf", 18)
font_legend_num = ImageFont.truetype(f"{FONT_DIR}/arialbd.ttf", 20)
font_legend = ImageFont.truetype(f"{FONT_DIR}/arial.ttf", 20)
font_footer = ImageFont.truetype(f"{FONT_DIR}/arial.ttf", 15)

title = "Maßarbeit Vision — HX711-Anschluss"
sub = "LilyGO T-Display S3 · nur die tatsächlich verkabelten Header-Pins"
tb = cd.textbbox((0, 0), title, font=font_title)
cd.text(((W - (tb[2] - tb[0])) / 2, 24), title, font=font_title, fill="#1f2933")
sb = cd.textbbox((0, 0), sub, font=font_sub)
cd.text(((W - (sb[2] - sb[0])) / 2, 62), sub, font=font_sub, fill="#52606d")

img_x = (W - crop.width) // 2
canvas.paste(crop, (img_x, title_h))
cd.rectangle([img_x, title_h, img_x + crop.width, title_h + crop.height], outline="#d1d5db", width=1)

legend = [
    ("1", "#dc2626", "3V", "HX711 VCC (Plus)"),
    ("2", "#d97706", "GPIO 1", "HX711 DOUT (Daten)"),
    ("3", "#d97706", "GPIO 2", "HX711 SCK (Takt)"),
    ("4", "#1f2933", "G", "HX711 GND (Minus)"),
]
ly = title_h + crop.height + 24
for num, color, pinname, desc in legend:
    bx, by = 40, ly + 14
    br = 16
    cd.ellipse([bx - br, by - br, bx + br, by + br], fill=color, outline="#ffffff", width=2)
    tb2 = cd.textbbox((0, 0), num, font=font_legend_num)
    cd.text((bx - (tb2[2] - tb2[0]) / 2 - tb2[0], by - (tb2[3] - tb2[1]) / 2 - tb2[1]), num, font=font_legend_num, fill="#ffffff")
    cd.text((bx + 34, by - 13), pinname, font=font_legend_num, fill="#1f2933")
    cd.text((bx + 34 + 120, by - 13), desc, font=font_legend, fill="#52606d")
    ly += 42

fy = title_h + crop.height + legend_h + 10
footer = "Bildquelle: offizielles Pinout-Diagramm github.com/Xinyuan-LilyGO/T-Display-S3 (annotiert)"
fb = cd.textbbox((0, 0), footer, font=font_footer)
cd.text(((W - (fb[2] - fb[0])) / 2, fy), footer, font=font_footer, fill="#9aa5b1")

canvas.save("../massarbeit-vision-pinout.png")
print(canvas.size)
