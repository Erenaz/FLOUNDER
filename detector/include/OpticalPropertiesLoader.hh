#pragma once
#include <string>
#include <vector>
#include "globals.hh"

class G4Material;
class G4MaterialPropertiesTable;

struct WaterOpticsSummary {
  G4double lambda_min_nm = 0, lambda_max_nm = 0;
  size_t   npoints = 0;
  G4double n_min = 0, n_max = 0;
  G4double labs_min_m = 0, labs_max_m = 0;
  G4double lsca_min_m = 0, lsca_max_m = 0;
  std::vector<G4double> energy_grid; // in Geant4 energy units (ascending)
};

struct PMTSummary {
  G4double lambda_min_nm = 0, lambda_max_nm = 0;
  size_t   npoints = 0;
  G4double mean_qe_400_450 = 0; // (fraction, not %)
};

class OpticalPropertiesLoader {
public:
  // Build MPT for water from CSV: columns "lambda_nm, n, absLen_m, scatLen_m"
  // Returns a new MPT (caller owns) and fills summary (also returns energy grid used).
  static G4MaterialPropertiesTable* BuildWaterMPTFromCSV(
      const std::string& csv_path, WaterOpticsSummary& out);

  // Attach RINDEX=1.0 over 'energies' to vacuum-like materials (e.g. G4_Galactic).
  static void AttachVacuumRindex(G4Material* vacuum,
                                 const std::vector<G4double>& energies);

  // Load PMT QE(λ) from CSV: columns "lambda_nm, qe" (qe as 0..1 or %)
  // Computes ⟨QE⟩ in [400,450] nm and returns summary.
  static PMTSummary LoadPMTQE(const std::string& csv_path,
                               G4double mean_from_nm = 400.0,
                               G4double mean_to_nm   = 450.0);
};
