#!/usr/bin/env bash
set -euo pipefail

PYTHON_BIN="$(command -v python3)"
PYTHONPATH_ORIG="${PYTHONPATH-}"
PYENV_ROOT_ORIG="${PYENV_ROOT-}"
PYENV_VERSION_ORIG="${PYENV_VERSION-}"
PATH_ORIG="$PATH"

OUT=out/day2/qc
mkdir -p "$OUT"

source detector/GEANT4.sh

rootfile="$OUT/timing_burst.root"
macro="${TIMING_BURST_MACRO:-macros/detector/dev/timing_burst.mac}"
FLNDR_DIGI_STORE_ALL_SAMPLES=1 \
FLNDR_PMTHITS_OUT="$rootfile" \
  detector/build/flndr --profile=day2 --quiet --summary_every=0 \
  --timing_opt_boundary_only \
  --optics=detector/config/optics_clear.yaml \
  --pmt=detector/config/pmt.yaml \
  --qe_flat=1 --threshold_pe=0 --gate_mode=off \
  "$macro"

env_cmd=( "PATH=$PATH_ORIG" "HOME=$HOME" "PYTHONPATH=$PYTHONPATH_ORIG" )
if [[ -n "$PYENV_ROOT_ORIG" ]]; then
  env_cmd+=( "PYENV_ROOT=$PYENV_ROOT_ORIG" )
fi
if [[ -n "$PYENV_VERSION_ORIG" ]]; then
  env_cmd+=( "PYENV_VERSION=$PYENV_VERSION_ORIG" )
fi

env -i "${env_cmd[@]}" "$PYTHON_BIN" detector/tools/qc/timing_sigma.py \
  --mode=gun --pmt_id=0 --event=0 \
  --out "$OUT/timing_sigma_burst.json" \
  "$rootfile"

echo "Wrote $OUT/timing_sigma_burst.json"
