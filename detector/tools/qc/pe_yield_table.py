#!/usr/bin/env python3
import json
import pathlib
import sys

import numpy as np
import uproot

rows = []
for arg in sys.argv[1:]:
    path = pathlib.Path(arg)
    if not path.exists():
        raise FileNotFoundError(f"Input file not found: {path}")
    with uproot.open(path) as f:
        hits = f["hits"]
        npe = hits["npe"].array(library="np")
    rows.append({"file": str(path), "total_PEs": float(np.sum(npe))})

print(json.dumps(rows, indent=2))
