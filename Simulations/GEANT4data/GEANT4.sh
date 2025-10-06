#!/bin/bash
# FLOUNDER-specific GEANT4 environment

# Force single-threaded mode to avoid ROOT threading issues
export G4FORCE_RUN_MANAGER_TYPE=Serial

export G4VIS_DEFAULT_DRIVER=TSG_OFFSCREEN

# Set up Homebrew ROOT
source /usr/local/Cellar/root/6.36.04/bin/thisroot.sh

# Set up GEANT4
export G4INSTALL="$HOME/Desktop/CodeRepository/GEANT4/geant4-v11.3.1-install"
source "$G4INSTALL/bin/geant4.sh"
export PATH="$G4INSTALL/bin:$PATH"

# Simulation-specific variables
export G4_GDML="/Users/jingyuanzhang/Desktop/AstroParticle/FLOUNDER/Simulations/fln_geo.gdml"
export G4_ROOTRACKER="/Users/jingyuanzhang/Desktop/AstroParticle/FLOUNDER/Simulations/Data/cc_only/ccmu_2k.gtrac.root"
export G4_ZSHIFT_MM=-20000

echo "FLOUNDER environment loaded (single-threaded mode)"