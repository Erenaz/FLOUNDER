#!/usr/bin/env python3
"""Estimate dark-rate contribution from hit flags."""
import argparse
import json
from pathlib import Path
import numpy as np
import uproot
import yaml


def load_config(cfg_path: Path):
    with cfg_path.open() as f:
        return yaml.safe_load(f) or {}


def main():
    parser = argparse.ArgumentParser(description="Dark-rate QC")
    parser.add_argument("input", help="ROOT file with hits")
    parser.add_argument("--pmt_cfg", default="detector/config/pmt.yaml")
    parser.add_argument("--out", default="out/day2/qc/dark_rate.json")
    args = parser.parse_args()

    cfg = load_config(Path(args.pmt_cfg))
    dark_rate_hz = cfg.get("dark_rate_hz", cfg.get("DARK_rate_HZ", 0.0))
    gate_ns = cfg.get("gate_ns", cfg.get("GATE_ns", 0.0))

    with uproot.open(args.input) as fin:
        tree = None
        for name in ("hits", "digits", "DigiHits"):
            if name in fin:
                tree = fin[name]
                break
        if tree is None:
            raise RuntimeError("hits tree not found")
        flags = tree["flags"].array(library="np") if "flags" in tree else None
        pmts = tree["pmt"].array(library="np") if "pmt" in tree else None
        if flags is None or pmts is None:
            raise RuntimeError("flags or pmt branch missing")

        dark_hits = np.count_nonzero((flags & 0x1) > 0)
        unique_pmts = np.unique(pmts)
        n_pmts = len(unique_pmts)
        events = len(tree["event"].array(library="np")) if "event" in tree else tree.num_entries
        measured_per_event = dark_hits / events if events > 0 else 0.0
        expected_per_event = dark_rate_hz * (gate_ns * 1e-9) * n_pmts

    out = {
        "measured": measured_per_event,
        "expected": expected_per_event,
        "N_PMT": n_pmts,
        "dark_rate_Hz": dark_rate_hz,
        "gate_ns": gate_ns,
        "events": events,
    }

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(out, indent=2))
    print(f"[DARK_RATE] Wrote {out_path}")


if __name__ == "__main__":
    main()
