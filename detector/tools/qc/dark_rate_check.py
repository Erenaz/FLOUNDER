#!/usr/bin/env python3
import json
import sys

import numpy as np
import uproot

if len(sys.argv) != 2:
    raise SystemExit("Usage: dark_rate_check.py <hits.root>")

with uproot.open(sys.argv[1]) as f:
    flags = f["hits"]["flags"].array(library="np")

dark_hits = int(np.count_nonzero((flags & 0x1) != 0))
print(json.dumps({"dark_hits": dark_hits}, indent=2))
