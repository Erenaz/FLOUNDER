#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
source "$repo_root/detector/GEANT4.sh"

outdir="$repo_root/out/day2/qc"
mkdir -p "$outdir"
macro="$repo_root/macros/detector/dev/mu50_fast.mac"
rootfile="$outdir/mu50_fast_dark.root"

FLNDR_PMTHITS_OUT="$rootfile" \
  "$repo_root/detector/build/flndr" --profile=day2 --quiet \
  --optics=detector/config/optics_clear.yaml \
  --opt_enable=cerenkov,abs,boundary \
  "$macro"

python "$repo_root/detector/tools/qc/dark_rate.py" "$rootfile"
