# Week 2 Quality Checks

## Photon-gun sanity

Run:

```bash
source detector/GEANT4.sh
detector/build/flndr --profile=day1 --optics=detector/config/optics_clear.yaml \
  --opt_enable=boundary macros/detector/dev/one_photon_to_pmt.mac
```

Expected log lines:

```
[OPT_DBG] event=0 OpticalHits size=1
[PMTDigi] evt=0 raw=1 kept≈Bernoulli(QE)
```

## Ring uniformity

Run:

```bash
detector/build/flndr --profile=day1 --optics=detector/config/optics_clear.yaml \
  --opt_enable=boundary macros/detector/dev/ring_uniformity.mac > logs/ring_uniformity.log
python detector/tools/qc/ring_uniformity_parse.py logs/ring_uniformity.log
```

Produces `out/day2/qc/ring_uniformity.csv` with columns `pmt_id,phi_deg,opt_hits,raw,kept`.
(Polar plot optional.)

## Muon 50 GeV fast

```bash
detector/build/flndr --profile=day2 --quiet \
  --optics=detector/config/optics_clear.yaml --opt_enable=cerenkov,abs,boundary \
  macros/detector/dev/mu50_fast.mac > logs/mu50_fast.log
```

Results land under `out/day2/qc/` via the QC tools.

## QC summary table

| Check                 | Status | Result / Target                     | Artifact(s)                                 |
|----------------------|--------|-------------------------------------|---------------------------------------------|
| PE per meter         | ✅     | ~3.08×10^4 PE/m (20 evts)           | out/day2/qc/pe_yield.json (.csv)            |
| QE sweep ratio       | ✅     | 6.84 (≥ 3 expected)                 | out/day2/qc/qe_sweep.json                    |
| Timing σ (gun burst) | ✅     | 257.24 ps (target ≈ 269 ps)         | out/day2/qc/timing_sigma_gun.json           |
| Dark singles/event   | ✅     | 6.95 vs expected 7.37               | out/day2/qc/dark_rate.json                   |
| Ring uniformity      | ✅     | 48/48 aimed PMTs hit (opt_hits=1)   | out/day2/qc/ring_uniformity.csv (.png)      |
| Pre-loss yield (ctest)| ✅/—  | within ±30% of expectation          | ctest -R light_yield output                  |

Measured with photon-gun burst, QE=1, threshold=0, gate=off; `dt_min_ps≈2 fs` ⇒ no time quantization bias.

## QC scripts

- detector/tools/qc/run_pe_yield.sh
- detector/tools/qc/qe_sweep.sh
- detector/tools/qc/run_timing.sh
- detector/tools/qc/run_darks.sh
- detector/tools/qc/ring_uniformity_parse.py
