#!/usr/bin/env bash
set -euo pipefail

BIN="${1:-./flndr_day2}"
MACRO="${2:-macros/hc_headless.mac}"
OUTDIR="docs/day2"
TS="$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTDIR"

LOG="$OUTDIR/optics_init_${TS}.log"
MAN="$OUTDIR/manifest_${TS}.yaml"
CSUM="$OUTDIR/checksums_${TS}.txt"

# --- 0) Basic context
echo "# Day-2 optics run ($(date -u +"%Y-%m-%dT%H:%M:%SZ"))" | tee "$LOG"
echo "# Host: $(hostname)" | tee -a "$LOG"
echo "# PWD : $(pwd)"      | tee -a "$LOG"

# --- 1) Versions / env (non-sensitive subset)
{
  echo "## versions"
  command -v geant4-config >/dev/null 2>&1 && echo "geant4: $(geant4-config --version)"
  python3 - <<'PY'
try:
    import ROOT
    print("root  :", ROOT.gROOT.GetVersion())
except Exception as e:
    pass
PY
  echo "## env"
  env | grep -E '^(G4|GEANT4|ROOT|LHAPDF|PYTHIA|DYLD|LD|PATH)=' | sort
} >> "$LOG" 2>/dev/null || true

# --- 2) Checksums (GDML + optics CSVs)
{
  echo "## checksums (sha256)"
  if ls optics/*.csv >/dev/null 2>&1; then
    shasum -a 256 optics/*.csv
  fi
  if [ -f "Simulations/fln_geo.gdml" ]; then
    shasum -a 256 "Simulations/fln_geo.gdml"
  elif [ -f "fln_geo.gdml" ]; then
    shasum -a 256 "fln_geo.gdml"
  fi
} | tee "$CSUM" >> "$LOG"

# --- 3) Git info (if repo)
{
  echo "## git"
  git rev-parse --is-inside-work-tree >/dev/null 2>&1 && {
    echo "commit: $(git rev-parse HEAD)"
    echo "status:"
    git status -s
    echo "diffstat:"
    git diff --stat || true
  }
} >> "$LOG" || true

# --- 4) Run
echo "## run" >> "$LOG"
set +e
"$BIN" "$MACRO" 2>&1 | tee -a "$LOG"
RC="${PIPESTATUS[0]}"
set -e

# --- 5) Extract key optics lines to a YAML manifest
{
  echo "timestamp: \"$TS\""
  echo "binary: \"$BIN\""
  echo "macro: \"$MACRO\""
  echo "log: \"$(basename "$LOG")\""
  echo "checksums: \"$(basename "$CSUM")\""
  echo "optics:"
  awk '/^\[Optics\] Water optics:/ {print "  water: \""$0"\""}
       /^\[Optics\] PMT QE:/      {print "  pmt_qe: \""$0"\""}
       /^\[Optics\] Event optical photons created:/ {c++} END{print "  photon_events: " (c+0)}' "$LOG"
  echo "return_code: $RC"
} > "$MAN"

echo "Wrote:"
echo "  - $LOG"
echo "  - $CSUM"
echo "  - $MAN"
exit "$RC"
