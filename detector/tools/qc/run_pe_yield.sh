#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"

source "$repo_root/detector/GEANT4.sh"

outdir="$repo_root/out/day2/qc"
mkdir -p "$outdir"
rootfile="$outdir/mu50_fast.root"

cat <<'MAC' > "$outdir/mu50_fast.mac"
/fln/genMode gun
/run/initialize
/vis/disable
/gun/particle mu-
/gun/energy 50 GeV
/gun/position 0 0 -19850 mm
/gun/direction 0 0 1
/run/beamOn 20
MAC

if [[ ! -f "$rootfile" ]]; then
  echo "[run_pe_yield] Generating $rootfile ..."
  FLNDR_PMTHITS_OUT="$rootfile" \
    "$repo_root/detector/build/flndr" --profile=day2 --quiet --summary_every=0 \
    --optics=detector/config/optics_clear.yaml \
    --opt_enable=cerenkov,abs,boundary \
    --threshold_pe=0 \
    "$outdir/mu50_fast.mac"
else
  echo "[run_pe_yield] Reusing existing $rootfile"
fi

python "$repo_root/detector/tools/qc/pe_yield.py" "$rootfile"
