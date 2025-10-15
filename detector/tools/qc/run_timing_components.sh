#!/usr/bin/env bash
set -euo pipefail

OUT=out/day2/qc
mkdir -p "$OUT"

source detector/GEANT4.sh

run_component() {
  local label="$1"
  local macro="$2"
  local enable_tts="$3"
  local enable_jitter="$4"

  local rootfile="$OUT/${label}.root"
  local json_out="$OUT/${label}.json"

  echo "[RUN_TIMING_COMPONENTS] Running ${label} (TTS=${enable_tts}, Jitter=${enable_jitter})"
  FLNDR_DIGI_STORE_ALL_SAMPLES=1 \
  FLNDR_PMTHITS_OUT="$rootfile" \
    detector/build/flndr \
      --profile=day2 \
      --quiet \
      --timing_opt_boundary_only \
      --optics=detector/config/optics_clear.yaml \
      --pmt=detector/config/pmt.yaml \
      --qe_flat=1 \
      --threshold_pe=0 \
      --enable_tts="$enable_tts" \
      --enable_jitter="$enable_jitter" \
      "$macro"

  python3 detector/tools/qc/timing_sigma.py \
    --mode=gun \
    --pmt_id=0 \
    --event=0 \
    --out "$json_out" \
    "$rootfile"
}

# TTS only
run_component "timing_tts_only" "macros/detector/dev/timing_burst_tts.mac" 1 0

# Jitter only
run_component "timing_jitter_only" "macros/detector/dev/timing_burst_jit.mac" 0 1

# Both
run_component "timing_both" "macros/detector/dev/timing_burst_both.mac" 1 1

echo "[RUN_TIMING_COMPONENTS] Results written to $OUT"
