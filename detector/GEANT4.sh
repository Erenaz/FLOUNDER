#!/bin/bash
# FLOUNDER-specific GEANT4 environment

# Force single-threaded mode to avoid ROOT threading issues
export G4FORCE_RUN_MANAGER_TYPE=Serial

export G4VIS_DEFAULT_DRIVER=TSG_OFFSCREEN

# Set up Homebrew ROOT
source /usr/local/Cellar/root/6.36.04/bin/thisroot.sh
export ROOTPCHDIR="$ROOTSYS/lib/root"
export ROOTMODULEINFODIR="$ROOTSYS/lib/root"

# Ensure SDKROOT points to a valid macOS SDK (avoids cling module map failures)
if command -v xcrun >/dev/null 2>&1; then
  _sdk_path="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null)"
  if [ -n "$_sdk_path" ]; then
    export SDKROOT="$_sdk_path"
  fi
  unset _sdk_path
fi

export C_INCLUDE_PATH="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include${C_INCLUDE_PATH:+:$C_INCLUDE_PATH}"
export CPLUS_INCLUDE_PATH="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include/c++/v1${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}"

# Disable ROOT clang modules to tolerate missing CommandLineTools headers
export ROOT_DISABLE_MODULES=1
export ROOT_MODULES=off
export ROOT_CXXMODULES=0

# Set up GEANT4
export G4INSTALL="$HOME/Desktop/CodeRepository/GEANT4/geant4-v11.3.1-install"
source "$G4INSTALL/bin/geant4.sh"
export PATH="$G4INSTALL/bin:$PATH"

# Simulation-specific variables
export G4_GDML="/Users/jingyuanzhang/Desktop/AstroParticle/FLOUNDER/configs/fln_geo.gdml"
export G4_ROOTRACKER="/Users/jingyuanzhang/Desktop/AstroParticle/FLOUNDER/data/gSeaGen_Data/cc_only/ccmu_2k.gtrac.root"
export G4_ZSHIFT_MM=-20000
export FLNDR_OPTICS_DIR="/Users/jingyuanzhang/Desktop/AstroParticle/FLOUNDER/detector/optics"

echo "FLOUNDER environment loaded (single-threaded mode)"
