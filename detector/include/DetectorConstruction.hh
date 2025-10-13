#pragma once

#include <string>

#include <G4GDMLParser.hh>
#include <G4String.hh> 
#include <G4VUserDetectorConstruction.hh>

class DetectorConstruction : public G4VUserDetectorConstruction {
public:
  explicit DetectorConstruction(const G4String& gdmlPath,
                                std::string opticsConfigPath = std::string(),
                                int checkOverlapsN = 0,
                                double qeOverride = std::numeric_limits<double>::quiet_NaN(),
                                double qeFlat = std::numeric_limits<double>::quiet_NaN());
  ~DetectorConstruction() override = default;
  G4VPhysicalVolume* Construct() override;
private:
  G4String fGdmlPath;
  std::string fOpticsPath;
  int fCheckOverlapsN = 0;
  double fQeOverride = std::numeric_limits<double>::quiet_NaN();
  double fQeFlat = std::numeric_limits<double>::quiet_NaN();
  G4GDMLParser fParser;
};
