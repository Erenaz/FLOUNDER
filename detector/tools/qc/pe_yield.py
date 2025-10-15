#!/usr/bin/env python3
"""
Summarise photoelectron yield per metre of track for FLOUNDER ROOT outputs.

Usage:
    python detector/tools/qc/pe_yield.py file1.root [file2.root ...]

Outputs:
    JSON: out/day2/qc/pe_yield.json
    CSV : out/day2/qc/pe_yield.csv
"""

import argparse
import json
import math
import os
import sys
from pathlib import Path
from typing import List, Tuple, Optional

import numpy as np
import uproot


_track_warning_emitted = False


def ensure_outdir(path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    return path


def read_manifest(file: uproot.ReadOnlyDirectory) -> Optional[dict]:
    if "run_manifest;1" not in file:
        return None
    named = file["run_manifest;1"]
    try:
        return json.loads(named.member("fTitle"))
    except Exception:
        return None


def estimate_track_length(tree: uproot.models.TTree.Model_TTree) -> Tuple[float, bool]:
    """Attempt to extract track length in metres from the tree; fallback to 40 m."""
    for branch in ("trackLen_m", "track_length_m", "trackLen"):
        if branch in tree.keys():
            arr = tree[branch].array(library="np")
            if arr.size > 0:
                value = float(np.mean(arr))
                if math.isfinite(value) and value > 0.0:
                    return value, True
    return 40.0, False  # fallback if not stored


def process_file(path: Path):
    global _track_warning_emitted

    with uproot.open(path) as file:
        manifest = read_manifest(file)

        tree = None
        for key in ("hits", "Hits", "digits", "DigiHits"):
            if key in file:
                tree = file[key]
                break
        if tree is None:
            raise RuntimeError(f"{path}: no hits/digits tree found.")

        npe = tree["npe"].array(library="np") if "npe" in tree else None
        npe = np.asarray(npe, dtype=float) if npe is not None else np.array([], dtype=float)
        total_pe = float(np.sum(npe))

        events = None
        if manifest:
            events = manifest.get("events")

        event_ids = None
        if "event" in tree:
            event_ids = tree["event"].array(library="np")
            events = int(np.unique(event_ids).size) if events is None else events

        if events is None:
            events = int(npe.size)

        track_length, found = estimate_track_length(tree)
        if not found and not _track_warning_emitted:
            sys.stderr.write("[pe_yield] WARNING: track length branch missing; assuming 40.0 m\n")
            _track_warning_emitted = True

        pe_per_m = total_pe / track_length if track_length > 0 else 0.0

        return {
            "file": str(path),
            "events": events,
            "totalPE": total_pe,
            "trackLen_m": track_length,
            "PE_per_m": pe_per_m,
        }


def main(argv: List[str]) -> None:
    parser = argparse.ArgumentParser(description="Compute PE yield per metre from ROOT hit files.")
    parser.add_argument("inputs", nargs="+", help="ROOT files containing 'hits' tree.")
    parser.add_argument("--json", default="out/day2/qc/pe_yield.json")
    parser.add_argument("--csv", default="out/day2/qc/pe_yield.csv")
    args = parser.parse_args(argv)

    records = [process_file(Path(inp)) for inp in args.inputs]

    json_path = ensure_outdir(Path(args.json))
    with json_path.open("w", encoding="utf-8") as out_json:
        json.dump(records, out_json, indent=2)

    csv_path = ensure_outdir(Path(args.csv))
    with csv_path.open("w", encoding="utf-8") as out_csv:
        out_csv.write("file,events,totalPE,trackLen_m,PE_per_m\n")
        for rec in records:
            out_csv.write(
                f"{rec['file']},{rec['events']},{rec['totalPE']:.6f},"
                f"{rec['trackLen_m']:.6f},{rec['PE_per_m']:.6f}\n"
            )

    print(f"[PE_YIELD] Wrote JSON: {json_path}")
    print(f"[PE_YIELD] Wrote CSV : {csv_path}")


if __name__ == "__main__":
    import sys
    main(sys.argv[1:])
