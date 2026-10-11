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
rects = [tuple(map(int, m)) for m in re.findall(r"^RECT (-?\d+) (-?\d+) (\d+) (\d+)", log, re.M)]
if not rects:
    sys.exit("no RECT line in probe.log")


def black_fraction(img):
    px = img.convert("RGB").getdata()
    return sum(1 for r, g, b in px if max(r, g, b) <= BLACK_MAX) / len(px)


# One row per frame: the black fraction of each editor instance, judged
# against that instance's own static frame.
frames = sorted(out.glob("[0-9]*.png"))
results = {}
rows_img = {}
for f in frames:
    shot = Image.open(f)
    crops = [shot.crop((x, y, x + w, y + h)) for x, y, w, h in rects]
    row = Image.new("RGB", (sum(c.width for c in crops), max(c.height for c in crops)), "white")
    left = 0
    for c in crops:
        row.paste(c, (left, 0))
        left += c.width
    row.save(out / f"editor-{f.name}")
    rows_img[f.stem] = row
    results[f.stem] = [black_fraction(c) for c in crops]

static = results.get("00-static", [0.0] * len(rects))
for i, (x, y, w, h) in enumerate(rects):
    print(f"editor {i + 1} rect: {x},{y} {w}x{h}")
for name, fracs in results.items():
    print(f"  {name:12s} " + "  ".join(f"{fr:6.1%}" for fr in fracs) + " black")

verdicts = []
for i in range(len(rects)):
    worst_name, worst = max(((n, fr[i]) for n, fr in results.items()), key=lambda kv: kv[1])
    if static[i] > 0.9:
        verdicts.append(f"BLACK EDITOR (editor {i + 1}: static frame did not paint)")
    elif worst > static[i] + REPAINT_MARGIN:
        verdicts.append(
            f"BLACK REGIONS (editor {i + 1}: {worst_name} is {worst:.1%} black "
            f"vs {static[i]:.1%} static)")
print("VERDICT: " + ("; ".join(verdicts) if verdicts else "OK"))

# One contact sheet per run makes eyeballing builds side by side easy.
crops = list(rows_img.values())
cols = 4 if len(rects) == 1 else 2
w = max(c.width for c in crops)
h = max(c.height for c in crops)
rows = (len(crops) + cols - 1) // cols
sheet = Image.new("RGB", (cols * w, rows * h), "white")
for i, c in enumerate(crops):
    sheet.paste(c, ((i % cols) * w, (i // cols) * h))
sheet.save(out / "contact-sheet.png")

# run.sh reports a non-zero exit as a failed check.
sys.exit(1 if verdicts else 0)
