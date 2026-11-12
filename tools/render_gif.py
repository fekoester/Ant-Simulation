"""Turn the PPM frames written by `ants --dump-dir DIR` into an animated GIF."""

import argparse
import glob
import os

from PIL import Image


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("frames", help="directory with frame_*.ppm")
    ap.add_argument("--out", default="docs/ants.gif")
    ap.add_argument("--fps", type=float, default=12)
    ap.add_argument("--colors", type=int, default=64)
    args = ap.parse_args()

    paths = sorted(glob.glob(os.path.join(args.frames, "frame_*.ppm")))
    if not paths:
        raise SystemExit(f"no frames in {args.frames}")
    frames = [
        Image.open(p)
        .convert("RGB")
        .quantize(colors=args.colors, method=Image.Quantize.MEDIANCUT)
        for p in paths
    ]
    frames[0].save(
        args.out,
        save_all=True,
        append_images=frames[1:],
        duration=int(1000 / args.fps),
        loop=0,
    )
    print(f"wrote {args.out} ({len(frames)} frames)")


if __name__ == "__main__":
    main()
