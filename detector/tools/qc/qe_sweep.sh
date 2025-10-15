#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
source "$repo_root/detector/GEANT4.sh"

outdir="$repo_root/out/day2/qc"
mkdir -p "$outdir"
macro="${QC_QE_SWEEP_MACRO:-$repo_root/macros/detector/dev/mu50_fast.mac}"

real_root="$outdir/mu50_fast_real.root"
qe1_root="$outdir/mu50_fast_qe1.root"

echo "[QE_SWEEP] Running real QE sample..."
rm -f "$real_root"
FLNDR_PMTHITS_OUT="$real_root" \
  "$repo_root/detector/build/flndr" --profile=day2 --quiet --summary_every=0 \
  --optics=detector/config/optics_clear.yaml \
  --opt_enable=cerenkov,abs,boundary \
  --threshold_pe=0 \
  "$macro"

echo "[QE_SWEEP] Running flat QE=1 sample..."
rm -f "$qe1_root"
FLNDR_PMTHITS_OUT="$qe1_root" \
  "$repo_root/detector/build/flndr" --profile=day2 --quiet --summary_every=0 \
  --optics=detector/config/optics_clear.yaml \
  --opt_enable=cerenkov,abs,boundary \
  --qe_flat=1 \
  --threshold_pe=0 \
  "$macro"

json_detail="$outdir/qe_sweep_detail.json"
csv_detail="$outdir/qe_sweep_detail.csv"
python "$repo_root/detector/tools/qc/pe_yield.py" \
  --json "$json_detail" --csv "$csv_detail" \
  "$real_root" "$qe1_root"

python - "$repo_root" <<'PY'
import json
from pathlib import Path
import sys

repo = Path(sys.argv[1])
detail_path = repo / 'out/day2/qc/qe_sweep_detail.json'
data = json.loads(detail_path.read_text())
if len(data) < 2:
    raise SystemExit(f"Expected >=2 entries from {detail_path}, got {len(data)}.")

by_file = {Path(entry["file"]).name: entry for entry in data}
try:
    pe_real = by_file['mu50_fast_real.root']['totalPE']
    pe_qe1 = by_file['mu50_fast_qe1.root']['totalPE']
except KeyError as ex:
    raise SystemExit(f"Missing expected entry in detail JSON: {ex}") from ex

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
