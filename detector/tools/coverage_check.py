#!/usr/bin/env python3
"""
Coverage and uniformity checker for PMT calibration scans.

Usage:
  python coverage_check.py --hits hits.root --positions positions.json

Expected inputs:
  - ROOT file with tree 'hits' (branches: event, pmt, t_ns, npe, flags).
  - JSON/CSV listing the (x,y) position used for each event id.

The script computes per-PMT hit probabilities and PE yields per transverse
position. It prints min/median/max PE and flags PMTs with <5% of the median
hit count. Optionally writes a heatmap PNG.
"""

import argparse
import json
import math
import os
from collections import defaultdict

import numpy as np

try:
    import uproot
except ImportError as exc:
    raise SystemExit("coverage_check.py requires uproot; pip install uproot") from exc

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError:
    plt = None


def load_positions(path):
    if not path:
        return {}
    if path.lower().endswith(".json"):
        with open(path, "r", encoding="utf-8") as fh:
            data = json.load(fh)
        return {int(k): tuple(v) for k, v in data.items()}
    raise ValueError(f"Unsupported positions file format: {path}")


def main():
    parser = argparse.ArgumentParser(description="PMT coverage checker")
    parser.add_argument("--hits", required=True, help="ROOT file with hits tree")
    parser.add_argument("--positions", help="JSON mapping event->(x_mm,y_mm)")
    parser.add_argument("--outdir", default="out/day2/qc", help="Directory for optional PNG")
    parser.add_argument("--tag", default="coverage", help="Label for outputs")
    parser.add_argument("--heatmap", action="store_true", help="Write heatmap PNG if matplotlib available")
    args = parser.parse_args()

    positions = load_positions(args.positions)

    with uproot.open(args.hits) as f:
        tree = f["hits"]
        events = tree["event"].array(library="np")
        pmts = tree["pmt"].array(library="np")
        npe = tree["npe"].array(library="np")

    unique_events = np.unique(events)
    n_events = len(unique_events)
    print(f"[INFO] Loaded {len(events)} hits across {n_events} events from {args.hits}")

    hits_per_pmt = defaultdict(int)
    pe_per_event = defaultdict(float)

    for evt, pmt_id, charge in zip(events, pmts, npe):
        hits_per_pmt[int(pmt_id)] += 1
        pe_per_event[int(evt)] += charge

    counts = np.array(list(hits_per_pmt.values()))
    median_hits = np.median(counts) if counts.size else 0.0
    flagged = [pid for pid, cnt in hits_per_pmt.items() if median_hits > 0 and cnt < 0.05 * median_hits]

    print(f"[COVERAGE] PMTs: {len(hits_per_pmt)} median_hits={median_hits:.2f}")
    if flagged:
        print(f"[WARN] {len(flagged)} PMTs below 5% of median hits: {flagged[:10]}{'...' if len(flagged)>10 else ''}")
    else:
        print("[COVERAGE] No PMTs below 5% of median hits.")

    pe_values = np.array(list(pe_per_event.values()))
    if pe_values.size:
        print(f"[PE] min={pe_values.min():.1f} median={np.median(pe_values):.1f} max={pe_values.max():.1f}")
    else:
        print("[PE] No PE data available.")

    if positions and plt is not None and args.heatmap:
        xs, ys, pes = [], [], []
        for evt, pe in pe_per_event.items():
            if evt in positions:
                x, y = positions[evt]
                xs.append(x)
                ys.append(y)
                pes.append(pe)
        if xs:
            xs = np.array(xs)
            ys = np.array(ys)
            pes = np.array(pes)
            outdir = args.outdir
            os.makedirs(outdir, exist_ok=True)
            fig, ax = plt.subplots(figsize=(6, 5))
            sc = ax.scatter(xs, ys, c=pes, cmap="viridis", s=200, edgecolor="k")
            ax.set_xlabel("x [mm]")
            ax.set_ylabel("y [mm]")
            ax.set_title(f"Total PE vs position ({args.tag})")
            plt.colorbar(sc, ax=ax, label="Total PE")
            outpath = os.path.join(outdir, f"{args.tag}_heatmap.png")
            plt.tight_layout()
            fig.savefig(outpath, dpi=150)
            plt.close(fig)
            print(f"[INFO] Saved heatmap to {outpath}")
        else:
            print("[INFO] Positions file provided but no matching events found for heatmap.")
    elif args.heatmap and plt is None:
        print("[WARN] matplotlib not available; skipping heatmap.")


if __name__ == "__main__":
    main()
