#pragma once
#include <G4VUserDetectorConstruction.hh>
#include <G4GDMLParser.hh>
#include <G4String.hh> 

class DetectorConstruction : public G4VUserDetectorConstruction {
public:
  explicit DetectorConstruction(const G4String& gdmlPath);
  ~DetectorConstruction() override = default;
  G4VPhysicalVolume* Construct() override;
private:
  G4String fGdmlPath;
  G4GDMLParser fParser;
};
