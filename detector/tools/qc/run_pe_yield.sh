#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"

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

$repo_root/detector/build/flndr --profile=day2 --quiet \
  --optics=detector/config/optics_clear.yaml \
  --opt_enable=cerenkov,abs,boundary \
  "$outdir/mu50_fast.mac"

python "$repo_root/detector/tools/qc/pe_yield.py" "$rootfile"
