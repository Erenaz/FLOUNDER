#!/usr/bin/env bash
set -euo pipefail

OUT=out/day2/qc
mkdir -p "$OUT"

source detector/GEANT4.sh

# 1) Muon 50 GeV (fast, quiet)
detector/build/flndr --profile=day2 --quiet \
  macros/detector/dev/mu50_fast.mac \
  --optics=detector/config/optics_clear.yaml \
  --pmt=detector/config/pmt.yaml

# 2) PE/m
bash detector/tools/qc/run_pe_yield.sh

# 3) QE sweep (real vs flat=1)
bash detector/tools/qc/qe_sweep.sh

# 4) Timing (gun burst, gate OFF, QE=1, threshold=0)
bash detector/tools/qc/run_timing_burst.sh

# 5) Darks
bash detector/tools/qc/run_darks.sh

# 6) Ring uniformity (assumes you already ran the macro; parse last log)
python3 detector/tools/qc/ring_uniformity_parse.py logs/ring_uniformity.log \
  --out "$OUT/ring_uniformity.csv" --png "$OUT/ring_uniformity.png"

# 7) Pre-loss ctest
ctest -R light_yield --output-on-failure || true

echo "QC artifacts in $OUT"
