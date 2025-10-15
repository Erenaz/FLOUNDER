#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

export TIMING_BURST_MACRO="$repo_root/macros/detector/dev/timing_burst_500.mac"

rm -f out/day2/qc/timing_sigma_gun.json out/day2/qc/timing_sigma_burst.json
bash detector/tools/qc/run_timing_burst.sh

python3 - <<'PY'
import json
from pathlib import Path

for candidate in (
    Path("out/day2/qc/timing_sigma_burst.json"),
    Path("out/day2/qc/timing_sigma_gun.json"),
):
    if candidate.exists():
        path = candidate
        break
else:
    raise SystemExit("No timing sigma JSON found.")

data = json.loads(path.read_text())
sigma = float(data.get("sigma_t_ps", float("nan")))
if not (180.0 <= sigma <= 320.0):
    raise SystemExit(f"Timing sigma out of range: {sigma} ps")
print(f"[TEST] Timing sigma OK: {sigma:.3f} ps")
PY
