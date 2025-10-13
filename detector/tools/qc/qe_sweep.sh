#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
source "$repo_root/detector/GEANT4.sh"

outdir="$repo_root/out/day2/qc"
mkdir -p "$outdir"
macro="$repo_root/macros/detector/dev/mu50_fast.mac"

real_root="$outdir/mu50_fast_real.root"
qe1_root="$outdir/mu50_fast_qe1.root"

echo "[QE_SWEEP] Running real QE sample..."
FLNDR_PMTHITS_OUT="$real_root" \
  "$repo_root/detector/build/flndr" --profile=day2 --quiet \
  --optics=detector/config/optics_clear.yaml \
  --opt_enable=cerenkov,abs,boundary \
  "$macro"

echo "[QE_SWEEP] Running flat QE=1 sample..."
FLNDR_PMTHITS_OUT="$qe1_root" \
  "$repo_root/detector/build/flndr" --profile=day2 --quiet \
  --optics=detector/config/optics_clear.yaml \
  --opt_enable=cerenkov,abs,boundary \
  --qe_flat=1.0 \
  "$macro"

json_detail="$outdir/qe_sweep_detail.json"
csv_detail="$outdir/qe_sweep_detail.csv"
python "$repo_root/detector/tools/qc/pe_yield.py" \
  --json "$json_detail" --csv "$csv_detail" \
  "$real_root" "$qe1_root"

python - <<'PY'
import json
from pathlib import Path
import sys
repo = Path(sys.argv[1])
with open(repo/'out/day2/qc/qe_sweep_detail.json') as f:
    data = json.load(f)
if len(data) != 2:
    raise SystemExit("Expected exactly two entries from pe_yield")
pe_real = data[0]['totalPE']
pe_qe1 = data[1]['totalPE']
ratio = pe_qe1 / pe_real if pe_real else float('nan')
summary = {
    'PE_real': pe_real,
    'PE_QE1': pe_qe1,
    'ratio': ratio
}
out_path = repo / 'out/day2/qc/qe_sweep.json'
out_path.write_text(json.dumps(summary, indent=2))
print(f"[QE_SWEEP] Wrote summary JSON: {out_path}")
PY
