#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
source "$repo_root/detector/GEANT4.sh"

outdir="$repo_root/out/day2/qc"
mkdir -p "$outdir"
macro="$repo_root/macros/detector/dev/timing_burst.mac"
rootfile="$outdir/mu50_fast_timing_gun.root"

echo "[RUN_TIMING_GUN] Generating timing probe sample..."
FLNDR_DIGI_STORE_ALL_SAMPLES=1 \
FLNDR_PMTHITS_OUT="$rootfile" \
  "$repo_root/detector/build/flndr" --profile=day2 --quiet --summary_every=0 \
  --optics=detector/config/optics_clear.yaml \
  --timing_opt_boundary_only \
  --qe_flat=1 \
  --threshold_pe=0 \
  "$macro"

python "$repo_root/detector/tools/qc/timing_sigma.py" \
  --mode=gun \
  --out "$outdir/timing_sigma_gun.json" \
  "$rootfile"
