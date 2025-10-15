#!/usr/bin/env python3
"""
Compute timing spreads from FLOUNDER digitized hits.

Modes
-----
gun  : assume one PMT is repeatedly aimed at. Use *all* hits for that PMT across events and
       return σ of (t - min(t)). Suitable for timing_probe.mac.
muon : replicate the legacy behaviour (earliest hit per PMT) with an optional TOF subtraction
       if a reference position and PMT coordinate map are provided.
"""

import argparse
import csv
import json
import math
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

import numpy as np
import uproot
import yaml

C_MM_PER_NS = 299.792458


def load_pmt_config(path: Path) -> Dict:
    if not path.exists():
        return {}
    with path.open() as f:
        try:
            return yaml.safe_load(f) or {}
        except yaml.YAMLError:
            return {}


def find_hits_tree(file: uproot.ReadOnlyDirectory):
    for name in ("hits", "digits", "DigiHits"):
        if name in file:
            return file[name]
    return None


def extract_arrays(tree):
    time_branch = None
    for candidate in ("t_ns", "time"):
        if candidate in tree:
            time_branch = candidate
            break
    if time_branch is None:
        raise RuntimeError("Tree has no 't_ns' or 'time' branch")

    times = tree[time_branch].array(library="np")
    if time_branch == "time":
        time_ns = np.asarray(times, dtype=float)
    else:
        time_ns = np.asarray(times, dtype=float)
    if "pmt" not in tree or "event" not in tree:
        raise RuntimeError("hits tree must contain 'pmt' and 'event' branches")
    pmts = tree["pmt"].array(library="np")
    events = tree["event"].array(library="np")
    return time_ns, np.asarray(pmts, dtype=int), np.asarray(events, dtype=int)


def parse_ref(ref_str: Optional[str]) -> Optional[Tuple[float, float, float]]:
    if not ref_str:
        return None
    parts = [p.strip() for p in ref_str.split(",")]
    if len(parts) != 3:
        raise ValueError("Reference position must have three comma-separated values")
    return tuple(float(p) for p in parts)


def load_pmt_positions(path: Optional[Path]) -> Dict[int, Tuple[float, float, float]]:
    if not path:
        return {}
    if not path.exists():
        raise FileNotFoundError(f"PMT map not found: {path}")
    positions: Dict[int, Tuple[float, float, float]] = {}
    with path.open() as f:
        reader = csv.DictReader(f)
        lowered = {name.lower(): name for name in reader.fieldnames or []}
        required = ["pmt", "x", "y", "z"]
        if not all(col in lowered for col in required):
            raise ValueError("PMT map must contain columns: pmt,x,y,z")
        for row in reader:
            idx = int(float(row[lowered["pmt"]]))
            positions[idx] = (
                float(row[lowered["x"]]),
                float(row[lowered["y"]]),
                float(row[lowered["z"]]),
            )
    return positions


def compute_sigma_gun(files: Iterable[Path], target_pmt: int, target_event: int) -> Dict:
    collected: List[float] = []

    for path in files:
        with uproot.open(path) as fin:
            tree = find_hits_tree(fin)
            if not tree:
                continue
            times, pmts, events = extract_arrays(tree)
            if times.size == 0:
                continue
            mask = (pmts == target_pmt) & (events == target_event)
            if np.any(mask):
                collected.extend(times[mask].tolist())

    n_hits = len(collected)
    if n_hits < 100:
        raise RuntimeError(
            f"Not enough hits for timing σ (need ≥100, got {n_hits}). "
            "Did you run timing_burst.mac with QE=1 & threshold=0?"
        )

    t_ns = np.asarray(collected, dtype=float)
    t_ps = t_ns * 1e3
    sorted_ps = np.sort(t_ps, axis=None)
    diffs = np.diff(sorted_ps)
    if diffs.size:
      unique_diffs = np.unique(np.round(diffs, 6))
      positive = unique_diffs[unique_diffs > 0]
      dt_min = float(positive.min()) if positive.size else 0.0
    else:
      dt_min = 0.0
    dt_ps = t_ps - t_ps.mean()
    sigma_ps = float(np.std(dt_ps, ddof=0))
    if dt_min > 0.0 and sigma_ps > 0.0 and dt_min > (sigma_ps / 5.0):
      print(
        f"[TIMING_SIGMA] WARNING: Timing appears quantized at ~{dt_min:.3f} ps; consider disabling rounding or increasing digitizer sampling rate.",
        file=sys.stderr,
      )

    return {
        "mode": "gun",
        "pmt_id": target_pmt,
        "event": target_event,
        "n_hits": n_hits,
        "sigma_t_ps": sigma_ps,
        "dt_min_ps": dt_min,
    }


def compute_sigma_muon(
    files: Iterable[Path],
    positions: Dict[int, Tuple[float, float, float]],
    ref: Optional[Tuple[float, float, float]],
    n_eff: float,
) -> Dict:
    earliest: Dict[int, float] = {}

    for path in files:
        with uproot.open(path) as fin:
            tree = find_hits_tree(fin)
            if not tree:
                continue
            times, pmts, _ = extract_arrays(tree)
            if times.size == 0:
                continue
            for t, p in zip(times, pmts):
                val = earliest.get(p)
                if val is None or t < val:
                    earliest[p] = float(t)

    if not earliest:
        return {
            "mode": "muon",
            "sigma_t_ps": 0.0,
            "n_pmts": 0,
            "tof_correction_applied": False,
        }

    values = np.asarray([earliest[p] for p in earliest], dtype=float)
    correction_applied = False
    if positions:
        if not ref:
            print("[TIMING_SIGMA] WARNING: PMT positions supplied but --ref not set; skipping TOF correction.", file=sys.stderr)
        else:
            corrected = []
            for p, t in earliest.items():
                pos = positions.get(p)
                if not pos:
                    continue
                dx = pos[0] - ref[0]
                dy = pos[1] - ref[1]
                dz = pos[2] - ref[2]
                distance = math.sqrt(dx * dx + dy * dy + dz * dz)
                tof = (n_eff * distance) / C_MM_PER_NS
                corrected.append(t - tof)
            if corrected:
                values = np.asarray(corrected, dtype=float)
                correction_applied = True
    else:
        print("[TIMING_SIGMA] WARNING: No PMT position map supplied; TOF correction skipped.", file=sys.stderr)

    sigma_ps = float(np.std(values) * 1e3)
    return {
        "mode": "muon",
        "sigma_t_ps": sigma_ps,
        "n_pmts": int(values.size),
        "tof_correction_applied": correction_applied,
    }


def main(argv: List[str]) -> None:
    parser = argparse.ArgumentParser(description="Compute timing spreads from digitized hits")
    parser.add_argument("inputs", nargs="+", help="ROOT files with hits")
    parser.add_argument("--out", default="out/day2/qc/timing_sigma.json")
    parser.add_argument("--pmt-cfg", "--pmt", dest="pmt_cfg", default="detector/config/pmt.yaml")
    parser.add_argument("--mode", choices=("gun", "muon"), default="gun")
    parser.add_argument("--pmt_id", type=int, default=0, help="Target PMT for gun mode")
    parser.add_argument("--event", type=int, default=0, help="Target event for gun mode")
    parser.add_argument("--ref", help="Reference (x,y,z) in mm for muon TOF subtraction")
    parser.add_argument("--pmt-map", help="CSV with columns pmt,x,y,z for muon TOF subtraction")
    parser.add_argument("--n-eff", type=float, default=1.33, help="Effective refractive index (muon mode)")
    opts = parser.parse_args(argv)

    pmt_cfg = load_pmt_config(Path(opts.pmt_cfg))
    result = {
        "mode": opts.mode,
        "pmt_id": opts.pmt_id,
        "event": opts.event,
        "TTS_sigma_ps": pmt_cfg.get("TTS_sigma_ps", pmt_cfg.get("tts_sigma_ps")),
        "elec_jitter_ps": pmt_cfg.get("elec_jitter_ps"),
    }

    if opts.mode == "gun":
        payload = compute_sigma_gun((Path(p) for p in opts.inputs), opts.pmt_id, opts.event)
    else:
        ref = parse_ref(opts.ref)
        positions = load_pmt_positions(Path(opts.pmt_map)) if opts.pmt_map else {}
        payload = compute_sigma_muon((Path(p) for p in opts.inputs), positions, ref, opts.n_eff)
        if ref:
            payload["ref_mm"] = {"x": ref[0], "y": ref[1], "z": ref[2]}
        if positions:
            payload["pmt_map"] = str(Path(opts.pmt_map).resolve())

    result.update(payload)

    out_path = Path(opts.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(result, indent=2))
    print(f"[TIMING_SIGMA] Wrote {out_path}")


if __name__ == "__main__":
    import sys

    main(sys.argv[1:])
