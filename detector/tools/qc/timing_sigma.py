#!/usr/bin/env python3
"""
Compute per-PMT timing spread from FLOUNDER ROOT hits.

Usage:
    python detector/tools/qc/timing_sigma.py hits.root [hits2.root]
Outputs JSON at out/day2/qc/timing_sigma.json with:
  {sigma_t_ps, n_pmts, TTS_ps, jitter_ps}
"""

import argparse
import json
from pathlib import Path
from typing import List

import numpy as np
import uproot
import yaml


def load_pmt_config(path: Path):
    if not path.exists():
        return {}
    with path.open() as f:
        try:
            return yaml.safe_load(f) or {}
        except yaml.YAMLError:
            return {}


def extract_sigma(tree):
    if "time" not in tree:
        raise RuntimeError("Tree has no 'time' branch")
    times = tree["time"].array(library="np")
    pmts = tree["pmt"].array(library="np") if "pmt" in tree else np.arange(len(times))
    if len(times) == 0:
        return 0.0, 0
    earliest = {}
    for t, p in zip(times, pmts):
        if p not in earliest or t < earliest[p]:
            earliest[p] = t
    values = np.array(list(earliest.values()))
    sigma_ns = float(np.std(values))
    return sigma_ns * 1e3, len(values)  # convert to ps


def main(args: List[str]) -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("inputs", nargs="+", help="ROOT files with hits")
    parser.add_argument("--out", default="out/day2/qc/timing_sigma.json")
    parser.add_argument("--pmt", default="detector/config/pmt.yaml")
    opts = parser.parse_args(args)

    pmt_cfg = load_pmt_config(Path(opts.pmt))
    result = {
        "sigma_t_ps": 0.0,
        "n_pmts": 0,
        "TTS_ps": pmt_cfg.get("TTS_sigma_ps", pmt_cfg.get("tts_sigma_ps", None)),
        "jitter_ps": pmt_cfg.get("elec_jitter_ps", None),
    }

    sigmas = []
    counts = []
    for path in opts.inputs:
        with uproot.open(path) as file:
            tree = None
            for name in ("hits", "digits", "DigiHits"):
                if name in file:
                    tree = file[name]
                    break
            if not tree:
                continue
            sigma_ps, npmts = extract_sigma(tree)
            if npmts > 0:
                sigmas.append(sigma_ps)
                counts.append(npmts)

    if sigmas:
        weights = np.array(counts)
        result["sigma_t_ps"] = float(np.average(sigmas, weights=weights))
        result["n_pmts"] = int(np.sum(counts))

    out_path = Path(opts.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(result, indent=2))
    print(f"[TIMING_SIGMA] Wrote {out_path}")


if __name__ == "__main__":
    import sys
    main(sys.argv[1:])
