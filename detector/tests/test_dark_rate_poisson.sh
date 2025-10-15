#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

export QC_DARKS_MACRO="$repo_root/macros/detector/dev/mu50_fast_5.mac"

rm -f out/day2/qc/dark_rate.json
bash detector/tools/qc/run_darks.sh

python3 - <<'PY'
import json
from pathlib import Path

data = json.loads(Path("out/day2/qc/dark_rate.json").read_text())
measured = float(data["measured_per_event"]["mean"])
expected = float(data["expected_per_event"])
if expected <= 0.0:
    raise SystemExit("Expected value must be positive.")
rel = abs(measured - expected) / expected
if rel > 0.3:
    raise SystemExit(f"Dark rate deviation too large: measured={measured}, expected={expected}, rel={rel:.3f}")
print(f"[TEST] Dark rate deviation OK: rel={rel:.3f}")
PY
