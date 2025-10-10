#!/usr/bin/env bash
set -euo pipefail

OUT=out/day2/qc
LOGDIR=logs/day2
CAL=out/day2/calib_mu_50.root
mkdir -p "$OUT" "$LOGDIR" out/day2

# 1) Build & run calibration set
FLNDR_PMTHITS_OUT="$CAL" \
  detector/build/flndr --profile=day2 macros/detector/day2/calib_mu_50.mac \
  --optics=detector/config/optics_clear.yaml \
  --pmt=detector/config/pmt.yaml \
  --opt_enable=cherenkov,abs,rayleigh,boundary | tee "$LOGDIR/calib_mu_50.log"

# 2) Light-yield (pre-loss sanity)
ctest --test-dir detector/build -R light_yield --output-on-failure || true

# 3) PE yield with real QE
python3 detector/tools/qc/pe_yield_table.py "$CAL" > "$OUT/pe_yield_realQE.json"

# 3b) Repeat with QE=1 across the spectrum
QE1_CFG="$OUT/pmt_qe1.yaml"
python3 - "$QE1_CFG" <<'PY'
import pathlib, sys
out = pathlib.Path(sys.argv[1])
text = pathlib.Path("detector/config/pmt.yaml").read_text().splitlines()
lines = []
qe_line = "QE_curve: [" + ",".join(["1"]*16) + "]"
for line in text:
    stripped = line.strip()
    if stripped.startswith("QE_curve:"):
        indent = line[: len(line) - len(line.lstrip())]
        lines.append(f"{indent}{qe_line}")
    elif stripped.lower().startswith("qe_scale"):
        indent = line[: len(line) - len(line.lstrip())]
        lines.append(f"{indent}QE_scale: 1.0")
    else:
        lines.append(line)
out.write_text("\n".join(lines) + "\n")
PY

CAL_QE1="$OUT/calib_mu_50_qe1.root"
FLNDR_PMTHITS_OUT="$CAL_QE1" \
  detector/build/flndr --profile=day2 macros/detector/day2/calib_mu_50.mac \
  --optics=detector/config/optics_clear.yaml \
  --pmt="$QE1_CFG" \
  --opt_enable=cherenkov,abs,rayleigh,boundary | tee "$LOGDIR/calib_mu_50_qe1.log"
python3 detector/tools/qc/pe_yield_table.py "$CAL_QE1" > "$OUT/pe_yield_QE1.json"

# 4) Timing sigma
python3 detector/tools/qc/timing_sigma.py "$CAL" > "$OUT/timing_sigma.json"

# 5) Dark-rate summary
python3 detector/tools/qc/dark_rate_check.py "$CAL" > "$OUT/dark_rate.json"

echo "QC artifacts in $OUT"
