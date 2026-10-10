"""Crop each screenshot to the editor and report how much of it is black.

A fully black static frame is the #36 symptom (editor never paints). Sweep
frames that are much blacker than the static frame are the #40 symptom (black
rectangles over regions that repainted).
"""

import re
import sys
from pathlib import Path

from PIL import Image

BLACK_MAX = 10  # a channel value at or below this counts as black
REPAINT_MARGIN = 0.05  # sweep frame this much blacker than static = flicker

out = Path(sys.argv[1])
log = (out / "probe.log").read_text()
m = re.search(r"^RECT (-?\d+) (-?\d+) (\d+) (\d+)", log, re.M)
if not m:
    sys.exit("no RECT line in probe.log")
x, y, w, h = map(int, m.groups())


def black_fraction(img):
    px = img.convert("RGB").getdata()
    return sum(1 for r, g, b in px if max(r, g, b) <= BLACK_MAX) / len(px)


frames = sorted(out.glob("[0-9]*.png"))
results = {}
for f in frames:
    crop = Image.open(f).crop((x, y, x + w, y + h))
    crop.save(out / f"editor-{f.name}")
    results[f.stem] = black_fraction(crop)

static = results.get("00-static", 0.0)
worst_name, worst = max(results.items(), key=lambda kv: kv[1])

print(f"editor rect: {x},{y} {w}x{h}")
for name, frac in results.items():
    print(f"  {name:12s} {frac:6.1%} black")

if static > 0.9:
    print("VERDICT: BLACK EDITOR (static frame did not paint)")
elif worst > static + REPAINT_MARGIN:
    print(f"VERDICT: BLACK REGIONS ({worst_name} is {worst:.1%} black vs {static:.1%} static)")
else:
    print("VERDICT: OK")

# One contact sheet per run makes eyeballing builds side by side easy.
crops = [Image.open(out / f"editor-{f.name}") for f in frames]
cols = 4
rows = (len(crops) + cols - 1) // cols
sheet = Image.new("RGB", (cols * w, rows * h), "white")
for i, c in enumerate(crops):
    sheet.paste(c, ((i % cols) * w, (i // cols) * h))
sheet.save(out / "contact-sheet.png")
