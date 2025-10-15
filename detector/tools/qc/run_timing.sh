#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
source "$repo_root/detector/GEANT4.sh"

outdir="$repo_root/out/day2/qc"
mkdir -p "$outdir"
macro="$repo_root/macros/detector/dev/mu50_fast.mac"
rootfile="$outdir/mu50_fast_timing.root"
base_run_root="$outdir/mu50_fast.root"

if [[ -f "$rootfile" ]]; then
  echo "[RUN_TIMING] Reusing existing $rootfile"
elif [[ -f "$base_run_root" ]]; then
  echo "[RUN_TIMING] Using existing $base_run_root (skipping new simulation)"
  rootfile="$base_run_root"
else
  echo "[RUN_TIMING] Generating $rootfile ..."
  FLNDR_PMTHITS_OUT="$rootfile" \
    "$repo_root/detector/build/flndr" --profile=day2 --quiet \
    --optics=detector/config/optics_clear.yaml \
    --opt_enable=cerenkov,abs,boundary \
    "$macro"
fi

python "$repo_root/detector/tools/qc/timing_sigma.py" \
  --mode=muon \
  "$rootfile"
