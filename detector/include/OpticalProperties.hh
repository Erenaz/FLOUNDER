#pragma once

#include <string>
#include <vector>

#include "globals.hh"

class G4Material;
class G4MaterialPropertiesTable;
class G4OpticalSurface;

struct WaterOpticsSummaryYaml {
  G4double lambdaMinNm = 0.0;
  G4double lambdaMaxNm = 0.0;
  size_t   npoints = 0;
  G4double rindexMin = 0.0;
  G4double rindexMax = 0.0;
  G4double absorptionMinMm = 0.0;
  G4double absorptionMaxMm = 0.0;
  G4double scatteringMinMm = 0.0;
  G4double scatteringMaxMm = 0.0;
};

struct PMTOpticsSummaryYaml {
  G4double lambdaMinNm = 0.0;
  G4double lambdaMaxNm = 0.0;
  size_t   npoints = 0;
  G4double meanQE400to450 = 0.0; // fraction
  G4double peakQE = 0.0;         // fraction
};

struct OpticalPropertiesResult {
  G4MaterialPropertiesTable* waterMPT = nullptr;
  G4OpticalSurface* wallSurface = nullptr;
  G4OpticalSurface* photocathodeSurface = nullptr;
  std::vector<G4double> energyGrid; // ascending
  std::vector<double> wavelength_nm;
  G4Material* photocathodeMaterial = nullptr;

  WaterOpticsSummaryYaml waterSummary;
  PMTOpticsSummaryYaml   pmtSummary;
};

class OpticalProperties {
public:
  static OpticalPropertiesResult LoadFromYaml(const std::string& path);
  static OpticalPropertiesResult LoadFromYaml(const std::string& path,
                                              double qeOverride);

  static void AttachVacuumRindex(G4Material* vacuum,
                                 const std::vector<G4double>& energies);

  static G4Material* BuildPhotocathodeMaterial(const std::vector<double>& wavelengthsNm,
                                               double rindex = 1.50);

  static void DumpWaterMPT(const G4Material* material,
                           const G4String& waterVolumeName);
};
