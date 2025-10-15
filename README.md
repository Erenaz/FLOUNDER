FLOUNDER
========

Forward LHC neutrinos — water Cherenkov study. This repo contains the detector simulation, macros, and QC tooling we’re using to calibrate optical photon transport and PMT response for the FLOUNDER concept. Build locally or via Singularity on HPC3; run profiles (day1/2/3) select fast vs. full physics.

What’s new (Week-2 wrap)
------------------------

**Photon-gun helpers** — aim a single optical photon directly at any photocathode:  
`/fln/aimAtPMT <id> [offset_mm] [energy_eV] [count]`  
Example macros: `macros/detector/dev/one_photon_to_pmt.mac`, `ring_probe.mac`, `ring_uniformity.mac`.

**Geometry registry** — each PMT photocathode stores its center and inward normal for targeting and ring scans.

**Quiet/verbose controls** — `--quiet`, `--opt_verbose=0..2`, `--summary_every=N` tame logs (no more GB-scale outputs).

**Digitizer overrides (for QC only)** — `--qe_flat`, `--qe_scale`, `--threshold_pe`, `--gate_mode={standard|centered|off}`, `--gate_ns_override`.

**Profiles —**

- **day1:** fast optics (Cherenkov + absorption + boundary, 50 photons/step)  
- **day2:** production/digitizer (same, digitizer enabled; defaults restored for production)  
- **day3:** full Rayleigh/Mie option

**QC scripts (under `detector/tools/qc/`):**  
`run_pe_yield.sh`, `qe_sweep.sh`, `run_timing_burst.sh`, `run_darks.sh`, `ring_uniformity_parse.py`, `week2_all.sh`, plus `ctest -R light_yield`.

Quick start
-----------

```
# 1) Environment
source detector/GEANT4.sh

# 2) Configure & build
cmake -S detector -B detector/build
cmake --build detector/build --target flndr

# 3) (Optional) photon-gun sanity — one aimed photon at PMT 0
detector/build/flndr --profile=day2 --quiet \
  macros/detector/dev/one_photon_to_pmt.mac \
  --optics=detector/config/optics_clear.yaml \
  --pmt=detector/config/pmt.yaml
```

Run profiles
------------

```
detector/build/flndr --profile=<day1|day2|day3> <macro.mac> [flags]
```

Helpful flags
-------------

`--quiet` silence step-level prints • `--opt_verbose=2` turn on boundary diagnostics • `--optics=…` and `--pmt=…` select YAMLs.

Configuration presets
---------------------

| Preset      | CLI                                           | Notes                                                  |
|-------------|-----------------------------------------------|--------------------------------------------------------|
| Clear water | `--optics=detector/config/optics_clear.yaml`  | Baseline (longest ABS/RAY lengths)                     |
| Lake water  | `--optics=detector/config/optics_lake.yaml`   | Shorter ABS, ~0.6× Rayleigh                            |
| Poor water  | `--optics=detector/config/optics_poor.yaml`   | ~¼ ABS, ~0.4× Rayleigh                                 |
| PMT model   | `--pmt=detector/config/pmt.yaml`              | QE peak ~28% near 400–450 nm; TTS/jitter, darks, threshold |

All selected YAMLs are echoed into the run manifest for reproducibility.

Developer utilities (Week-2)
----------------------------

**Aimed photon:**

- `macros/detector/dev/one_photon_to_pmt.mac` — single photon → OpticalHits=1, digitizer raw≈Bernoulli(QE).  
- `macros/detector/dev/ring_probe.mac` — walk PMTs 0–7 for instantaneous health check.  
- `macros/detector/dev/ring_uniformity.mac` — scan a full ring; parse with `ring_uniformity_parse.py` to CSV/PNG.

**Muon fast macro:**  
`macros/detector/dev/mu50_fast.mac` (50 GeV μ⁻, 20 events) for PE/m, timing, and dark-rate sweeps with quiet logging.

**Timing burst (QC only):**  
`macros/detector/dev/timing_burst.mac` + `--timing_opt_boundary_only --qe_flat=1 --threshold_pe=0 --gate_mode=off`  
→ many photons in one event; measures pure electronics σ (TTS⊕jitter).

Week-2 QC: how to run (one button)
----------------------------------

```
# Produces all Week-2 artifacts under out/day2/qc/
bash detector/tools/qc/week2_all.sh
```

**What it does:**

- Runs `mu50_fast.mac` (quiet) with day2 profile.  
- PE/m → `out/day2/qc/pe_yield.json` (+ CSV).  
- QE sweep (real QE vs QE=1) → `out/day2/qc/qe_sweep.json`.  
- Timing σ (gun burst) → `out/day2/qc/timing_sigma_gun.json`.  
- Dark singles → `out/day2/qc/dark_rate.json`.  
- Ring uniformity parse → `out/day2/qc/ring_uniformity.csv/png`.  
- Pre-loss Cherenkov regression: `ctest -R light_yield`.

Week-2 results (current)
------------------------

| Check                        | Status | Result / Target                           | Artifact                               |
|------------------------------|--------|-------------------------------------------|----------------------------------------|
| PE per meter (μ=50 GeV)      | ✅     | 3.08×10⁴ PE/m, 20 evts (stable ±11%)      | out/day2/qc/pe_yield.json              |
| QE sweep ratio (QE=1 / real) | ✅     | 6.84 (≥3 expected)                         | out/day2/qc/qe_sweep.json              |
| Timing σ (gun burst)         | ✅     | 257.24 ps (target ≈ 269 ps; ±10–20% band) | out/day2/qc/timing_sigma_gun.json      |
| Dark singles / event         | ✅     | 6.95 vs expected 7.37                      | out/day2/qc/dark_rate.json             |
| Ring uniformity (aimed)      | ✅     | 48/48 PMTs report opt_hits=1               | out/day2/qc/ring_uniformity.csv/png    |
| Pre-loss Cherenkov (ctest)   | ✅     | within ±30% of expectation                 | ctest -R light_yield output            |

**Notes:**

- Timing σ measured with QC overrides (QE=1, threshold=0, gate off) to isolate electronics.  
- `dt_min_ps ≈ 0.0022 ps` in burst timing ⇒ no quantization bias.  
- Production runs restore defaults (standard gate, real QE, thresholds).

Typical commands
----------------

**Muon fast (quiet):**

```
detector/build/flndr --profile=day2 --quiet \
  macros/detector/dev/mu50_fast.mac \
  --optics=detector/config/optics_clear.yaml \
  --pmt=detector/config/pmt.yaml
```

**QE sweep (ratio):**

```
bash detector/tools/qc/qe_sweep.sh
# writes out/day2/qc/qe_sweep.json
```

**Timing burst (QC):**

```
bash detector/tools/qc/run_timing_burst.sh
# writes out/day2/qc/timing_sigma_gun.json
```

**Dark singles:**

```
bash detector/tools/qc/run_darks.sh
# writes out/day2/qc/dark_rate.json
```

**Ring uniformity (then parse):**

```
detector/build/flndr --profile=day2 --quiet macros/detector/dev/ring_uniformity.mac
python3 detector/tools/qc/ring_uniformity_parse.py logs/ring_uniformity.log \
  --out out/day2/qc/ring_uniformity.csv --png out/day2/qc/ring_uniformity.png
```

Troubleshooting
---------------

- **Huge logs** — use `--quiet` and avoid `/tracking/verbose` unless debugging; `--opt_verbose=2` only for 1-event diagnostics.  
- **No PMT hits** — confirm dielectric–dielectric at water↔photocathode, photocathode has RINDEX, SD is attached to PMT_cathode_log, and the aimed photon points to a real PMT (use `/fln/aimAtPMT`).  
- **QE override doesn’t change PEs** — ensure overrides are applied in the digitizer (`[PMT.QE] effective: …` appears in header).  
- **Timing σ too small** — your gate truncates negatives; for QC run with `--gate_mode=off` (production remains standard).

Repo layout (high-level)
------------------------

```
detector/
  src, include/…          # GEANT4 app + digitizer + SD
  build/                  # CMake build tree
  config/
    optics_*.yaml         # water optical presets
    pmt.yaml              # PMT QE, TTS, jitter, darks, threshold
  tools/qc/               # QC scripts (PE/m, QE sweep, timing, darks, ring)
macros/
  detector/dev/           # photon-gun, ring, timing, muon fast
  detector/day1, day2/    # profile-specific runs
out/day2/qc/              # Week-2 artifacts (JSON/CSV/PNG)
logs/                     # small logs (use --quiet)
```

Reproducibility & tagging
-------------------------

All runs write a manifest that records profile, optics/PMT YAMLs, and CLI overrides.

Week-2 checkpoint tag: `v0.2-week2-optics-pmt` (recommended).
