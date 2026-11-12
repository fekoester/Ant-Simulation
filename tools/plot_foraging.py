"""Plot results/*_seed*.csv from tools/experiment.sh: food delivered and colony size, pheromones vs random walk."""

import argparse
import csv
import glob
import re
from collections import defaultdict

import matplotlib.pyplot as plt


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default="results")
    ap.add_argument("--out", default="docs/foraging.png")
    args = ap.parse_args()

    runs = defaultdict(list)  # mode -> list of dict(column -> list)
    for path in sorted(glob.glob(f"{args.results}/*_seed*.csv")):
        mode = re.sub(r"_seed\d+\.csv$", "", path.split("/")[-1])
        with open(path) as f:
            rows = list(csv.DictReader(f))
        runs[mode].append({k: [float(r[k]) for r in rows] for k in rows[0]})
    if not runs:
        raise SystemExit("no results")

    labels = {"pheromones": "pheromone following", "random_walk": "random walk only"}
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.2))
    for mode, rs in sorted(runs.items()):
        steps = rs[0]["step"]
        for ax, key, title in zip(
            axes, ["delivered", "ants"], ["food delivered", "ants"], strict=True
        ):
            mean = [sum(r[key][i] for r in rs) / len(rs) for i in range(len(steps))]
            lo = [min(r[key][i] for r in rs) for i in range(len(steps))]
            hi = [max(r[key][i] for r in rs) for i in range(len(steps))]
            (line,) = ax.plot(
                steps, mean, label=f"{labels.get(mode, mode)} (mean of {len(rs)})"
            )
            ax.fill_between(steps, lo, hi, color=line.get_color(), alpha=0.2)
            ax.set_title(title)
            ax.set_xlabel("step")
            ax.grid(alpha=0.3)
            ax.legend()
        final = [r["delivered"][-1] for r in rs]
        print(
            f"{mode}: delivered after {int(steps[-1])} steps: mean {sum(final) / len(final):.0f}, "
            f"min {min(final):.0f}, max {max(final):.0f}"
        )
    fig.tight_layout()
    fig.savefig(args.out, dpi=110)
    print("wrote", args.out)


if __name__ == "__main__":
    main()
