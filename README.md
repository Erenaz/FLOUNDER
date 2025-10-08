# FLOUNDER
Forward LHC neutrinos — water Cherenkov study.
This repo tracks code, macros, and SLURM jobs. Build & run via Singularity on HPC3.

## Detector quick start
- Configure your environment with `source detector/GEANT4.sh`.
- Configure and build once with `cmake -S detector -B detector/build` and `cmake --build detector/build --target flndr`.
- Launch simulations via `detector/build/flndr [--profile=<day1|day2|day3>] [macro.mac]`. The profile toggles run-time cuts while keeping a single executable.
- Macros now live under `macros/detector/`; e.g. `macros/detector/day1/hc.mac` or the 50 GeV muon shot `macros/single_mu.mac`.
