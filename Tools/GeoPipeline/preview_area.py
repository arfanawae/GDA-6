"""Renders provinces + roads + border-crossing signs of a Roads_<name>.json to a PNG for a quick visual check."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
DATA = HERE.parents[1] / "Content" / "_Downtown" / "Data"
COLORS = {"motorway": (230, 80, 60), "trunk": (240, 150, 50), "primary": (250, 220, 90)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--name", default="TestArea")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--px", type=int, default=1400)
    args = parser.parse_args()

    roads = json.loads((DATA / "Roads" / f"Roads_{args.name}.json").read_text(encoding="utf-8"))
    provinces = json.loads((DATA / "Provinces" / "DT_Provinces.json").read_text(encoding="utf-8"))
    b = roads["bounds"]
    w = args.px
    h = round(w * (b["MaxX"] - b["MinX"]) / (b["MaxY"] - b["MinY"]))

    def px(x, y):
        return ((y - b["MinY"]) / (b["MaxY"] - b["MinY"]) * w, (1 - (x - b["MinX"]) / (b["MaxX"] - b["MinX"])) * h)

    img = Image.new("RGB", (w, h), (35, 45, 60))
    d = ImageDraw.Draw(img)
    for p in provinces:
        fill = (70, 95, 80) if p["Code"] in roads["area"] else (55, 65, 70)
        for ring in p["Rings"]:
            d.polygon([px(q["X"], q["Y"]) for q in ring["Points"]], fill=fill, outline=(200, 200, 200))
    for r in sorted(roads["roads"], key=lambda r: r["highway"] != "primary"):  # draw big roads on top
        cls = r["highway"].replace("_link", "")
        width = 1 if "_link" in r["highway"] else 2
        if max(p[2] for p in r["points"]) > 0:
            width += 1
        d.line([px(p[0], p[1]) for p in r["points"]], fill=COLORS.get(cls, (200, 200, 200)), width=width)
    for c in roads["crossings"]:
        x, y = px(c["x"], c["y"])
        d.ellipse([x - 4, y - 4, x + 4, y + 4], outline=(80, 200, 255), width=2)
    img.save(args.out)
    print(f"{args.out} {w}x{h}")


if __name__ == "__main__":
    main()
