# FLOUNDER
Forward LHC neutrinos — water Cherenkov study.
This repo tracks code, macros, and SLURM jobs. Build & run via Singularity on HPC3.

## Detector quick start
- Configure your environment with `source detector/GEANT4.sh`.
- Configure and build once with `cmake -S detector -B detector/build` and `cmake --build detector/build --target flndr`.
- Launch simulations via `detector/build/flndr [--profile=<day1|day2|day3>] [macro.mac]`. The profile toggles run-time cuts while keeping a single executable.
- Macros now live under `macros/detector/`; e.g. `macros/detector/day1/hc.mac` or the 50 GeV muon shot `macros/single_mu.mac`.

## Water presets
You can swap the water absorption / scattering model at runtime:

| Preset | Command line | Notes |
|--------|--------------|-------|
| Clear  | `--optics=detector/config/optics_clear.yaml` | Baseline (longest lengths) |
| Lake   | `--optics=detector/config/optics_lake.yaml`  | Shorter absorption / 0.6× Rayleigh |
| Poor   | `--optics=detector/config/optics_poor.yaml`  | Quarter absorption / 0.4× Rayleigh |

These files are verbatim YAMLs embedded in the output manifest, so production logs capture the exact curves used.

## Week-2: Optical Photon Transport & PMT Response

### Build & run (calibration)
```bash
source detector/GEANT4.sh
cmake -S detector -B detector/build && cmake --build detector/build --target flndr
mkdir -p logs/day2 out/day2
FLNDR_PMTHITS_OUT=out/day2/calib_mu_50.root \
  detector/build/flndr --profile=day2 macros/detector/day2/calib_mu_50.mac \
  --optics=detector/config/optics_clear.yaml \
  --pmt=detector/config/pmt.yaml \
  --opt_enable=cherenkov,abs,rayleigh,boundary | tee logs/day2/calib_mu_50.log
```

Header checklist (PASS lines)

- OPT_PHYS registered; water MPT keys: RINDEX, ABSLENGTH, RAYLEIGH (N=16 @ 300–600 nm)
- SURF photocathode: EFFICIENCY=0; REFLECTIVITY≈0.05
- PMTSD attached to PhotocathodeLV (nonzero thickness)
- PMTDigitizer: TTS=… ps, jitter=… ps, dark=… Hz @ gate=… ns, threshold=… npe

Outputs

- ROOT hits tree and manifest at `out/day2/calib_mu_50.root`
- QC artifacts in `out/day2/qc/` after running the QC script below.

Water presets

```
# Clear (default)
--optics=detector/config/optics_clear.yaml
# Lake water
--optics=detector/config/optics_lake.yaml
# Poor water
--optics=detector/config/optics_poor.yaml
```

Run the QC helper once you have the calibration ROOT file:

```bash
detector/tools/qc/qc_day2.sh
```

This populates `out/day2/qc/` with PE-yield, timing-sigma, and dark-rate JSON summaries for quick comparisons.
