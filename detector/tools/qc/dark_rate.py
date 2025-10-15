#!/usr/bin/env python3
"""Estimate dark-rate contribution from digitized hit flags."""

import argparse
import json
from pathlib import Path
from typing import Dict, Tuple

import numpy as np
import uproot
import yaml


def load_yaml(path: Path) -> Dict:
    if not path.exists():
        return {}
    with path.open() as handle:
        data = yaml.safe_load(handle) or {}
    return data


def find_hits_tree(file: uproot.ReadOnlyDirectory):
    for name in ("hits", "digits", "DigiHits"):
        if name in file:
            return file[name]
    return None


def fetch_config_params(cfg_path: Path, manifest: Dict) -> Tuple[float, float]:
    cfg = load_yaml(cfg_path)
    dark_rate = cfg.get("dark_rate_Hz")
    gate = cfg.get("gate_ns")

    if (dark_rate is None or gate is None) and manifest:
        pmt_contents = manifest.get("pmt_contents")
        if pmt_contents:
            try:
                manifest_yaml = yaml.safe_load(pmt_contents) or {}
            except yaml.YAMLError as exc:
                raise RuntimeError(f"Failed to parse pmt_contents from manifest: {exc}") from exc
            dark_rate = dark_rate if dark_rate is not None else manifest_yaml.get("dark_rate_Hz")
            gate = gate if gate is not None else manifest_yaml.get("gate_ns")

    if dark_rate is None or gate is None:
        raise RuntimeError("Missing dark_rate_Hz or gate_ns in PMT config/manifest")

    return float(dark_rate), float(gate)


def load_manifest(file: uproot.ReadOnlyDirectory) -> Dict:
    if "run_manifest;1" not in file:
        return {}
    named = file["run_manifest;1"]
    try:
        title = named.member("fTitle")
        return json.loads(title)
    except Exception:
        return {}


def compute_dark_stats(tree) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    if "flags" not in tree or "event" not in tree or "pmt" not in tree:
        raise RuntimeError("hits tree must contain 'flags', 'event', and 'pmt' branches")

    flags = tree["flags"].array(library="np").astype(np.int64)
    events = tree["event"].array(library="np").astype(np.int64)
    pmts = tree["pmt"].array(library="np").astype(np.int64)
    return flags, events, pmts


def per_event_counts(events: np.ndarray, flags: np.ndarray) -> np.ndarray:
    dark_mask = (flags & 0x1) != 0
    if events.size == 0:
        return np.zeros(0, dtype=float)
    min_evt = int(events.min())
    max_evt = int(events.max())
    counts = np.zeros(max_evt - min_evt + 1, dtype=float)
    np.add.at(counts, events - min_evt, dark_mask.astype(float))
    return counts


def main() -> None:
    parser = argparse.ArgumentParser(description="Dark-rate QC")
    parser.add_argument("input", help="ROOT file with hits")
    parser.add_argument("--pmt-cfg", default="detector/config/pmt.yaml")
    parser.add_argument("--out", default="out/day2/qc/dark_rate.json")
    args = parser.parse_args()

    input_path = Path(args.input)
    if not input_path.exists():
        raise FileNotFoundError(f"Input file not found: {input_path}")

    with uproot.open(input_path) as fin:
        manifest = load_manifest(fin)
        dark_rate_hz, gate_ns = fetch_config_params(Path(args.pmt_cfg), manifest)

        tree = find_hits_tree(fin)
        if tree is None:
            raise RuntimeError("hits tree not found in input file")

        flags, events, pmts = compute_dark_stats(tree)

    counts = per_event_counts(events, flags)
    measured_mean = float(counts.mean()) if counts.size else 0.0
    measured_std = float(counts.std(ddof=0)) if counts.size else 0.0

    unique_pmts = np.unique(pmts)
    n_pmts = int(unique_pmts.size)

    expected_per_event = dark_rate_hz * (gate_ns * 1e-9) * n_pmts

    result = {
        "measured_per_event": {
            "mean": measured_mean,
            "std": measured_std,
            "events": int(counts.size),
        },
        "expected_per_event": expected_per_event,
        "N_PMT": n_pmts,
        "dark_rate_Hz": dark_rate_hz,
        "gate_ns": gate_ns,
    }

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(result, indent=2))
    print(f"[DARK_RATE] Wrote {out_path}")


if __name__ == "__main__":
    main()
