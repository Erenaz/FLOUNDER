#include "DetectorConstruction.hh"
#include "OpticalInit.hh"

#include <G4GDMLParser.hh>
#include <G4LogicalVolume.hh>
#include <G4LogicalVolumeStore.hh>
#include <G4NistManager.hh>
#include <G4RunManager.hh>
#include <G4SystemOfUnits.hh>
#include <G4VisAttributes.hh>
#include <G4Colour.hh>
#include <G4PhysicalVolumeStore.hh>
#include "G4LogicalVolumeStore.hh"

#include <algorithm>
#include <cctype>

#include <cstdlib>   // getenv
#include <string>

// Remap any GDML "Vacuum" materials to NIST G4_Galactic (cosmetic; keeps logs clean)
static int RemapGDMLVacuumToGalactic() {
  auto* gal = G4Material::GetMaterial("G4_Galactic");
  if (!gal) return 0;
  auto* store = G4LogicalVolumeStore::GetInstance();
  int n = 0;
  for (auto* lv : *store) {
    if (!lv) continue;
    auto* m = lv->GetMaterial();
    if (!m) continue;
    std::string name = m->GetName();
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    if (lower == "vacuum" || lower == "g4_vacuum") {
      lv->SetMaterial(gal);
      ++n;
    }
  }
  if (n>0) G4cout << "[Optics] Remapped " << n
                  << " logical volumes from GDML 'Vacuum' to G4_Galactic.\n";
  return n;
}

DetectorConstruction::DetectorConstruction(const G4String& gdmlPath)
  : fGdmlPath(gdmlPath) {}

G4VPhysicalVolume* DetectorConstruction::Construct() {
  if (fGdmlPath.empty()) {
    G4Exception("DetectorConstruction", "NoGDML", FatalException,
                "G4_GDML path not set (empty).");
  }

  // 1) Parse GDML and get world
  fParser.Read(fGdmlPath, /*validate=*/false);
  auto* worldPV = fParser.GetWorldVolume();
  if (!worldPV) {
    G4Exception("DetectorConstruction", "BadGDML", FatalException,
                "World volume is null after parsing GDML.");
  }
  auto* worldLV = worldPV->GetLogicalVolume();

  // Optionally remap any GDML 'Vacuum' LVs to G4_Galactic (quiet cosmetic warnings)
  RemapGDMLVacuumToGalactic();

  const char* odir_env = std::getenv("FLNDR_OPTICS_DIR");
  const std::string optics_dir = odir_env && *odir_env ? odir_env : "optics";
  const std::string water_csv  = optics_dir + "/water_properties.csv";
  const std::string pmt_csv    = optics_dir + "/pmt_qe.csv";

  // 2) Material overrides
  auto* nist = G4NistManager::Instance();

  // 2a) World -> true vacuum (G4_Galactic)
  if (auto* gal = nist->FindOrBuildMaterial("G4_Galactic")) {
    worldLV->SetMaterial(gal);
    G4cout << "[INFO] World material set to G4_Galactic\n";
  }

  // 2b) Can LV -> G4_WATER
  // GDML shows: <volume name="Detector"> with Water material.
  // witch to NIST G4_WATER so optics tables are standard.
  const char* canName = std::getenv("G4_CAN_LV");
  G4String targetCan = canName && *canName ? canName : "Detector";

  auto* lvStore = G4LogicalVolumeStore::GetInstance();
  auto* canLV = lvStore->GetVolume(targetCan, /*verbose=*/false);
  if (canLV) {
    if (auto* water = nist->FindOrBuildMaterial("G4_WATER")) {
      canLV->SetMaterial(water);
      G4cout << "[INFO] Set material of '" << targetCan << "' to G4_WATER\n";
    }
  } else {
    G4cout << "[WARN] Logical volume '" << targetCan
           << "' not found. Skipping can material override.\n";
  }

  G4VPhysicalVolume* canPV = nullptr;
  {
    auto* canLV = G4LogicalVolumeStore::GetInstance()->GetVolume(targetCan, /*verbose=*/false);
    if (canLV) {
      auto* pvStore = G4PhysicalVolumeStore::GetInstance();
      for (auto* pv : *pvStore) {
        if (pv && pv->GetLogicalVolume() == canLV) {
          canPV = pv;               // take the first instance we find
          break;
        }
      }
      if (!canPV) {
        G4cout << "[WARN] No physical instance found for can LV '" << targetCan
              << "'. Will attach optical tables but skip border surface.\n";
      }
    } else {
      G4cout << "[WARN] Can LV '" << targetCan
            << "' not found in LV store; skipping PV lookup.\n";
    }
  }

  // --- Attach optical tables, vacuum RINDEX, and build water↔world border surface ---
  if (!OpticalInit::ConfigureOptics(water_csv, pmt_csv, worldPV, canPV)) {
    G4Exception("DetectorConstruction", "Optics", FatalException,
                "Failed to configure optical properties.");
  }

  // 3) Simple visibility attributes (nice for ToolsSG screenshots)
  auto* worldVis = new G4VisAttributes(G4Colour(0.9, 0.9, 0.9, 0.03));
  worldVis->SetForceWireframe(true);
  worldLV->SetVisAttributes(worldVis);

  // If the can exists, tint it light blue (non-solid so we still see tracks)
  if (canLV) {
    auto* canVis = new G4VisAttributes(G4Colour(0.2, 0.5, 0.9, 0.2));
    canVis->SetForceSolid(false);
    canLV->SetVisAttributes(canVis);
  }

  return worldPV;
}
