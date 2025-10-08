#include "DetectorConstruction.hh"
#include "OpticalInit.hh"

#include <G4Box.hh>
#include <G4Colour.hh>
#include <G4GDMLParser.hh>
#include <G4LogicalBorderSurface.hh>
#include <G4LogicalVolume.hh>
#include <G4MaterialPropertiesTable.hh>
#include <G4NistManager.hh>
#include <G4PVPlacement.hh>
#include <G4OpticalSurface.hh>
#include <G4PhysicalVolumeStore.hh>
#include <G4RotationMatrix.hh>
#include <G4RunManager.hh>
#include <G4SDManager.hh>
#include <G4SystemOfUnits.hh>
#include <G4ThreeVector.hh>
#include <G4Tubs.hh>
#include <G4VisAttributes.hh>
#include "G4LogicalVolumeStore.hh"
#include "PMTSD.hh"
#include <G4String.hh>

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
  bool opticsConfigured = false;

  // 2a) World -> true vacuum (G4_Galactic)
  auto* gal = nist->FindOrBuildMaterial("G4_Galactic");
  if (gal) {
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

      G4VPhysicalVolume* canPV = nullptr;
      {
        auto* pvStore = G4PhysicalVolumeStore::GetInstance();
        for (auto* pv : *pvStore) {
          if (pv && pv->GetLogicalVolume() == canLV) {
            canPV = pv;
            break;
          }
        }
      }

      if (!OpticalInit::ConfigureOptics(water_csv, pmt_csv, worldPV, canPV)) {
        G4Exception("DetectorConstruction", "Optics", FatalException,
                    "Failed to configure optical properties.");
      }
      opticsConfigured = true;

      auto* cathSurface = OpticalInit::GetPhotocathodeSurface();

      auto* cathMat = nist->FindOrBuildMaterial("G4_Al");
      if (!cathMat) {
        G4cout << "[WARN] Material 'G4_Al' not found; skipping PMT tiling.\n";
      } else {
        const G4double kPmtRadius = 0.10 * m;
        const G4double kPmtThick  = 0.1 * mm;
        const G4double defaultZPitch = 0.50 * m;
        const G4int   defaultNPhi   = 48;

        auto* pmtSol = new G4Tubs("PMT_cathode_tubs", 0., kPmtRadius, kPmtThick / 2., 0., 360 * deg);
        auto* pmtLog = new G4LogicalVolume(pmtSol, cathMat, "PMT_cathode_log");

        auto* existingSD = canLV->GetSensitiveDetector();
        PMTSD* pmtSD = dynamic_cast<PMTSD*>(existingSD);
        if (!pmtSD) {
          pmtSD = new PMTSD("PMTSD");
          G4SDManager::GetSDMpointer()->AddNewDetector(pmtSD);
          canLV->SetSensitiveDetector(pmtSD);
        }

        const auto read_env_double = [](const char* name, double fallback) {
          if (const char* val = std::getenv(name)) {
            try { return std::stod(val); } catch (...) { return fallback; }
          }
          return fallback;
        };
        const auto read_env_int = [](const char* name, int fallback) {
          if (const char* val = std::getenv(name)) {
            try {
              int parsed = std::stoi(val);
              return parsed > 0 ? parsed : fallback;
            } catch (...) { return fallback; }
          }
          return fallback;
        };

        G4int pmtPlaced = 0;

        if (auto* waterBox = dynamic_cast<G4Box*>(canLV->GetSolid())) {
          const G4double halfX = waterBox->GetXHalfLength();
          const G4double halfY = waterBox->GetYHalfLength();
          const G4double halfZ = waterBox->GetZHalfLength();

          const G4double kZPitch = defaultZPitch;
          const G4double kXPitch = 0.375 * m;
          const G4double kYPitch = 0.375 * m;

          {
            G4double x = halfX - 0.5 * cm;
            for (G4double z = -halfZ + kZPitch; z <= halfZ - kZPitch; z += kZPitch) {
              for (G4double y = -halfY + kYPitch; y <= halfY - kYPitch; y += kYPitch) {
                auto rot = new G4RotationMatrix();
                rot->rotateY(90 * deg);
                auto* pmtPV = new G4PVPlacement(rot, {x, y, z}, pmtLog, "PMT", canLV, false, pmtPlaced++);
                if (cathSurface && canPV && pmtPV) {
                  G4String surfName = "PhotocathodeSurface_" + std::to_string(pmtPlaced);
                  new G4LogicalBorderSurface(surfName, canPV, pmtPV, cathSurface);
                }
              }
            }
          }
          {
            G4double x = -halfX + 0.5 * cm;
            for (G4double z = -halfZ + kZPitch; z <= halfZ - kZPitch; z += kZPitch) {
              for (G4double y = -halfY + kYPitch; y <= halfY - kYPitch; y += kYPitch) {
                auto rot = new G4RotationMatrix();
                rot->rotateY(-90 * deg);
                auto* pmtPV = new G4PVPlacement(rot, {x, y, z}, pmtLog, "PMT", canLV, false, pmtPlaced++);
                if (cathSurface && canPV && pmtPV) {
                  G4String surfName = "PhotocathodeSurface_" + std::to_string(pmtPlaced);
                  new G4LogicalBorderSurface(surfName, canPV, pmtPV, cathSurface);
                }
              }
            }
          }
          {
            G4double y = halfY - 0.5 * cm;
            for (G4double z = -halfZ + kZPitch; z <= halfZ - kZPitch; z += kZPitch) {
              for (G4double x = -halfX + kXPitch; x <= halfX - kXPitch; x += kXPitch) {
                auto rot = new G4RotationMatrix();
                rot->rotateX(-90 * deg);
                auto* pmtPV = new G4PVPlacement(rot, {x, y, z}, pmtLog, "PMT", canLV, false, pmtPlaced++);
                if (cathSurface && canPV && pmtPV) {
                  G4String surfName = "PhotocathodeSurface_" + std::to_string(pmtPlaced);
                  new G4LogicalBorderSurface(surfName, canPV, pmtPV, cathSurface);
                }
              }
            }
          }
          {
            G4double y = -halfY + 0.5 * cm;
            for (G4double z = -halfZ + kZPitch; z <= halfZ - kZPitch; z += kZPitch) {
              for (G4double x = -halfX + kXPitch; x <= halfX - kXPitch; x += kXPitch) {
                auto rot = new G4RotationMatrix();
                rot->rotateX(90 * deg);
                auto* pmtPV = new G4PVPlacement(rot, {x, y, z}, pmtLog, "PMT", canLV, false, pmtPlaced++);
                if (cathSurface && canPV && pmtPV) {
                  G4String surfName = "PhotocathodeSurface_" + std::to_string(pmtPlaced);
                  new G4LogicalBorderSurface(surfName, canPV, pmtPV, cathSurface);
                }
              }
            }
          }
          const int sdAttached = (canLV->GetSensitiveDetector() != nullptr) ? 1 : 0;
          G4cout << "[PMT] placed=" << pmtPlaced << " rings=NA perRing=NA wall=1 endcaps=0\n";
          G4cout << "[SENS] SD attached=" << sdAttached << G4endl;
        } else if (auto* tubs = dynamic_cast<G4Tubs*>(canLV->GetSolid())) {
          const G4double rOuter   = tubs->GetOuterRadius();
          const G4double zHalf    = tubs->GetZHalfLength();
          const G4double wallGap  = 5.0 * mm;
          const G4double zMargin  = kPmtRadius;
          G4double zPitch = read_env_double("FLNDR_PMT_ZPITCH_M", defaultZPitch / m) * m;
          if (zPitch <= 0.0) zPitch = defaultZPitch;
          const G4int nPhi = read_env_int("FLNDR_PMT_NPHI", defaultNPhi);
          const G4double radius = std::max(rOuter - wallGap, kPmtRadius + wallGap);

          int ringCount = 0;
          for (G4double z = -zHalf + zMargin; z <= zHalf - zMargin + 0.5 * zPitch; z += zPitch) {
            if (z > zHalf - zMargin) break;
            ++ringCount;
            for (int k = 0; k < nPhi; ++k) {
              const G4double phi = (2.0 * CLHEP::pi * k) / nPhi;
              const G4double x = radius * std::cos(phi);
              const G4double y = radius * std::sin(phi);
              auto rot = new G4RotationMatrix();
              rot->rotateZ(phi);
              rot->rotateY(90.0 * deg);
              auto* pmtPV = new G4PVPlacement(rot, {x, y, z}, pmtLog, "PMT", canLV, false, pmtPlaced++);
              if (cathSurface && canPV && pmtPV) {
                G4String surfName = "PhotocathodeSurface_" + std::to_string(pmtPlaced);
                new G4LogicalBorderSurface(surfName, canPV, pmtPV, cathSurface);
              }
            }
          }
        const int sdAttached = (canLV->GetSensitiveDetector() != nullptr) ? 1 : 0;
        G4cout << "[PMT] placed=" << pmtPlaced
               << " rings=" << ringCount
               << " perRing=" << nPhi
               << " wall=1 endcaps=0" << G4endl;
        G4cout << "[SENS] SD attached=" << sdAttached << G4endl;
      }
    }
    }
  } else {
    G4cout << "[WARN] Logical volume '" << targetCan
           << "' not found. Skipping can material override.\n";
  }

  if (!opticsConfigured) {
    if (!OpticalInit::ConfigureOptics(water_csv, pmt_csv, worldPV, nullptr)) {
      G4Exception("DetectorConstruction", "Optics", FatalException,
                  "Failed to configure optical properties.");
    }
  }

  // Emit geometry summary for the primary detector solid.
  if (auto* detLV = G4LogicalVolumeStore::GetInstance()->GetVolume(targetCan, /*recurse*/ true)) {
    auto* solid = detLV->GetSolid();
    if (auto* box = dynamic_cast<G4Box*>(solid)) {
      G4cout << "[GEOM] shape=Box Lx=" << (2.0 * box->GetXHalfLength() / mm) << "mm"
             << " Ly=" << (2.0 * box->GetYHalfLength() / mm) << "mm"
             << " Lz=" << (2.0 * box->GetZHalfLength() / mm) << "mm" << G4endl;
    } else if (auto* tubs = dynamic_cast<G4Tubs*>(solid)) {
      G4cout << "[GEOM] shape=Tubs Rmin=" << (tubs->GetInnerRadius() / mm) << "mm"
             << " Rmax=" << (tubs->GetOuterRadius() / mm) << "mm"
             << " Dz=" << (2.0 * tubs->GetZHalfLength() / mm) << "mm"
             << " dPhi=" << (tubs->GetDeltaPhiAngle() / deg) << "deg" << G4endl;
    } else if (solid) {
      G4cout << "[GEOM] shape=" << solid->GetEntityType() << G4endl;
    }
  }

  if (auto* surf = OpticalInit::GetPhotocathodeSurface()) {
    auto* mpt = surf->GetMaterialPropertiesTable();
    const int hasEff = (mpt && mpt->GetProperty("EFFICIENCY")) ? 1 : 0;
    const int hasRef = (mpt && mpt->GetProperty("REFLECTIVITY")) ? 1 : 0;
    G4cout << "[SURF] photocathode surface attached: EFFICIENCY=" << hasEff
           << " REFLECTIVITY=" << hasRef << G4endl;
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
