#!/usr/bin/env python3
"""
Parse a ring-uniformity Geant4 log and summarise PMT response.

Writes CSV with columns:
    pmt_id,phi_deg,opt_hits,raw,kept

Optionally produces a polar PNG of phi vs. kept-hit flag.
"""

import argparse
import csv
import math
import re
from pathlib import Path
from typing import List, Optional


PHOTON_RE = re.compile(
    r"\[PHOTON_GUN\]\s+pmt=(?P<pmt>\d+).*?phi_deg=(?P<phi>[-+]?[0-9]*\.?[0-9]+)"
)
OPT_HITS_RE = re.compile(
    r"\[OPT_DBG\].*OpticalHits size=(?P<hits>\d+)"
)
PMTDIGI_RE = re.compile(
    r"\[PMTDigi\]\s+evt=\d+\s+raw=(?P<raw>\d+)\s+kept=(?P<kept>\d+)"
)


def parse_log(lines: List[str]) -> List[dict]:
    entries: List[dict] = []

    def latest_missing(field: str) -> Optional[dict]:
        for entry in reversed(entries):
            if entry.get(field) is None:
                return entry
        return None

    for line in lines:
        line = line.strip()
        if not line:
            continue

        match = PHOTON_RE.search(line)
        if match:
            entries.append(
                {
                    "pmt_id": int(match.group("pmt")),
                    "phi_deg": float(match.group("phi")),
                    "opt_hits": None,
                    "raw": None,
                    "kept": None,
                }
            )
            continue

        match = OPT_HITS_RE.search(line)
        if match:
            entry = latest_missing("opt_hits")
            if entry is not None:
                entry["opt_hits"] = int(match.group("hits"))
            continue

        match = PMTDIGI_RE.search(line)
        if match:
            entry = latest_missing("raw")
            if entry is not None:
                entry["raw"] = int(match.group("raw"))
                entry["kept"] = int(match.group("kept"))
            continue

    # Normalise defaults
    for entry in entries:
        entry["opt_hits"] = int(entry.get("opt_hits") or 0)
        entry["raw"] = int(entry.get("raw") or 0)
        entry["kept"] = int(entry.get("kept") or 0)

    return entries


def write_csv(entries: List[dict], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(["pmt_id", "phi_deg", "opt_hits", "raw", "kept"])
        for entry in sorted(entries, key=lambda e: e["pmt_id"]):
            writer.writerow(
                [
                    entry["pmt_id"],
                    f"{entry['phi_deg']:.2f}",
                    entry["opt_hits"],
                    entry["raw"],
                    entry["kept"],
                ]
            )


def write_png(entries: List[dict], path: Path) -> None:
    try:
        import matplotlib.pyplot as plt
        import numpy as np
    except ImportError as exc:
        raise RuntimeError("matplotlib is required for --png output") from exc

    path.parent.mkdir(parents=True, exist_ok=True)

    phi = np.deg2rad([entry["phi_deg"] for entry in entries])
    kept = np.array([1 if entry["kept"] > 0 else 0 for entry in entries])

    fig = plt.figure(figsize=(6, 4))
    ax = fig.add_subplot(111, projection="polar")
    colors = np.where(kept > 0, "tab:orange", "tab:blue")
    ax.scatter(phi, kept, c=colors, s=60, edgecolors="k", linewidths=0.5)

    ax.set_rmax(1.2)
    ax.set_rticks([0, 1])
    ax.set_yticklabels(["0", "hit"])
    ax.set_title("Ring Uniformity (kept hits)", va="bottom")

    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Parse ring uniformity log.")
    parser.add_argument(
        "log",
        nargs="?",
        default="logs/ring_uniformity.log",
        help="Path to ring uniformity log file.",
    )
    parser.add_argument(
        "--out",
        default="out/day2/qc/ring_uniformity.csv",
        help="Output CSV path.",
    )
    parser.add_argument(
        "--png",
        help="Optional PNG path for polar plot (requires matplotlib).",
    )
    args = parser.parse_args()

    log_path = Path(args.log)
    if not log_path.exists():
        raise FileNotFoundError(f"Log file not found: {log_path}")

    with log_path.open(encoding="utf-8", errors="replace") as handle:
        entries = parse_log(handle.readlines())

    if not entries:
        raise RuntimeError(f"No PMT entries parsed from {log_path}")

    out_path = Path(args.out)
    write_csv(entries, out_path)
    print(f"[RING_UNIFORMITY] Wrote CSV: {out_path}")

    if args.png:
        png_path = Path(args.png)
        write_png(entries, png_path)
        print(f"[RING_UNIFORMITY] Wrote PNG: {png_path}")


if __name__ == "__main__":
    main()
