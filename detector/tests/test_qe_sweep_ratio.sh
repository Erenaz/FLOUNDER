#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

export QC_QE_SWEEP_MACRO="$repo_root/macros/detector/dev/mu50_fast_5.mac"

rm -f out/day2/qc/qe_sweep.json
bash detector/tools/qc/qe_sweep.sh

python3 - <<'PY'
import json
from pathlib import Path

data = json.loads(Path("out/day2/qc/qe_sweep.json").read_text())
ratio = float(data.get("ratio", float("nan")))
if not (ratio >= 3.0):
    raise SystemExit(f"QE sweep ratio too low: {ratio}")
print(f"[TEST] QE sweep ratio OK: {ratio:.3f}")
PY
