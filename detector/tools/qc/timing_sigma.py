#!/usr/bin/env python3
import json
import numpy as np
import sys

import uproot

if len(sys.argv) != 2:
    raise SystemExit("Usage: timing_sigma.py <hits.root>")

with uproot.open(sys.argv[1]) as f:
    arrays = f["hits"].arrays(["pmt", "t_ns"], library="np")

pmt = arrays["pmt"]
t_ns = arrays["t_ns"]
order = np.argsort(pmt)
pmt_sorted = pmt[order]
time_sorted = t_ns[order]
if pmt_sorted.size == 0:
    sigma_ps = 0.0
    n_pmts = 0
else:
    idx = np.concatenate(([0], 1 + np.where(np.diff(pmt_sorted) != 0)[0]))
    earliest = np.minimum.reduceat(time_sorted, idx)
    sigma_ps = float(np.std(earliest) * 1e3)
    n_pmts = int(earliest.size)

print(json.dumps({"sigma_t_ps": sigma_ps, "n_pmts": n_pmts}, indent=2))
