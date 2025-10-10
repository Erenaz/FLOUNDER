#include "DetectorConstruction.hh"
#include "globals.hh"
#include "OpticalProperties.hh"

#include <G4Box.hh>
#include <G4Colour.hh>
#include <G4GDMLParser.hh>
#include <G4GeometryManager.hh>
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
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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

static std::string fmt_range(double a, double b, int precision, const char* unit = "") {
  std::ostringstream os;
  os.setf(std::ios::fixed);
  os << std::setprecision(precision) << "[" << a << "," << b << "]" << unit;
  return os.str();
}

static const char* SurfaceTypeName(G4SurfaceType type) {
  switch (type) {
    case dielectric_metal: return "dielectric_metal";
    case dielectric_dielectric: return "dielectric_dielectric";
    case dielectric_LUT: return "dielectric_LUT";
    case dielectric_LUTDAVIS: return "dielectric_LUTDAVIS";
    case firsov: return "firsov";
    case x_ray: return "x_ray";
    default: return "unknown";
  }
}

static const char* SurfaceModelName(G4OpticalSurfaceModel model) {
  switch (model) {
    case glisur: return "glisur";
    case unified: return "unified";
    case LUT: return "lut";
    case DAVIS: return "davis";
    case dichroic: return "dichroic";
    default: return "unknown";
  }
}

static const char* SurfaceFinishName(G4OpticalSurfaceFinish finish) {
  switch (finish) {
    case polished: return "polished";
    case polishedfrontpainted: return "polishedfrontpainted";
    case polishedbackpainted: return "polishedbackpainted";
    case ground: return "ground";
    case groundfrontpainted: return "groundfrontpainted";
    case groundbackpainted: return "groundbackpainted";
    default: return "custom";
  }
}

DetectorConstruction::DetectorConstruction(const G4String& gdmlPath,
                                           std::string opticsConfigPath,
                                           int checkOverlapsN,
                                           double qeOverride)
  : fGdmlPath(gdmlPath),
    fOpticsPath(std::move(opticsConfigPath)),
    fCheckOverlapsN(checkOverlapsN),
    fQeOverride(qeOverride) {}

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

  std::string opticsPath = fOpticsPath;
  if (opticsPath.empty()) {
    if (const char* envConfig = std::getenv("FLNDR_OPTICS_CONFIG")) {
      opticsPath = envConfig;
    } else if (const char* envDir = std::getenv("FLNDR_OPTICS_DIR")) {
      opticsPath = std::string(envDir) + "/optics.yaml";
    } else {
      opticsPath = "detector/config/optics.yaml";
    }
  }

  OpticalPropertiesResult opticsTables;
  bool opticsLoaded = false;
  try {
    opticsTables = OpticalProperties::LoadFromYaml(opticsPath, fQeOverride);
    opticsLoaded = true;
  } catch (const std::exception& ex) {
    const std::string msg =
        "Failed to load optics config '" + opticsPath + "': " + ex.what();
    G4Exception("DetectorConstruction", "OpticsConfig", FatalException, msg.c_str());
    return worldPV;
  }

  // 2) Material overrides
  auto* nist = G4NistManager::Instance();

  // 2a) World -> true vacuum (G4_Galactic)
  auto* gal = nist->FindOrBuildMaterial("G4_Galactic");
  if (gal) {
    worldLV->SetMaterial(gal);
    G4cout << "[INFO] World material set to G4_Galactic\n";
    if (opticsLoaded) {
      OpticalProperties::AttachVacuumRindex(gal, opticsTables.energyGrid);
    }
  }

  auto* waterMaterial = nist->FindOrBuildMaterial("G4_WATER");
  if (!waterMaterial) {
    G4Exception("DetectorConstruction", "WaterMaterial", FatalException,
                "Failed to find or build material 'G4_WATER'.");
  } else if (opticsLoaded && opticsTables.waterMPT) {
    waterMaterial->SetMaterialPropertiesTable(opticsTables.waterMPT);
    auto* mpt = waterMaterial->GetMaterialPropertiesTable();
    auto* rindex   = mpt ? mpt->GetProperty("RINDEX")    : nullptr;
    auto* absl     = mpt ? mpt->GetProperty("ABSLENGTH") : nullptr;
    auto* rayleigh = mpt ? mpt->GetProperty("RAYLEIGH")  : nullptr;
    if (!rindex || !absl || !rayleigh) {
      G4Exception("DetectorConstruction", "Optics", FatalException,
                  "Water material properties table missing RINDEX/ABSLENGTH/RAYLEIGH.");
    }
    const auto nR = rindex->GetVectorLength();
    if (nR != absl->GetVectorLength() || nR != rayleigh->GetVectorLength()) {
      G4Exception("DetectorConstruction", "OpticsTableSize", FatalException,
                  "Water material property vectors have unequal lengths.");
    }
  }

  if (opticsLoaded) {
    const auto& ws = opticsTables.waterSummary;
    const auto& ps = opticsTables.pmtSummary;
    std::ostringstream waterLog;
    waterLog.setf(std::ios::fixed);
    waterLog << "[Optics] Water optics: λ=" << fmt_range(ws.lambdaMinNm, ws.lambdaMaxNm, 1, " nm")
             << " (N=" << ws.npoints << "); n=" << fmt_range(ws.rindexMin, ws.rindexMax, 4)
             << "; L_abs=" << fmt_range(ws.absorptionMinMm * 1e-3, ws.absorptionMaxMm * 1e-3, 1, " m")
             << "; L_scat=" << fmt_range(ws.scatteringMinMm * 1e-3, ws.scatteringMaxMm * 1e-3, 1, " m");
    G4cout << waterLog.str() << G4endl;

    std::ostringstream pmtLog;
    pmtLog.setf(std::ios::fixed);
    pmtLog << "[Optics] PMT QE: λ=" << fmt_range(ps.lambdaMinNm, ps.lambdaMaxNm, 1, " nm")
           << " (N=" << ps.npoints << "); <QE>_400–450nm = "
           << std::setprecision(1) << (ps.meanQE400to450 * 100.0)
           << " % peak=" << (ps.peakQE * 100.0) << " %";
    G4cout << pmtLog.str() << G4endl;
  }

  // 2b) Can LV -> G4_WATER
  // GDML shows: <volume name="Detector"> with Water material.
  // witch to NIST G4_WATER so optics tables are standard.
  const char* canName = std::getenv("G4_CAN_LV");
  G4String targetCan = canName && *canName ? canName : "Detector";

  auto* lvStore = G4LogicalVolumeStore::GetInstance();
  auto* canLV = lvStore->GetVolume(targetCan, /*verbose=*/false);
  if (canLV) {
    if (waterMaterial) {
      canLV->SetMaterial(waterMaterial);
      G4cout << "[INFO] Set material of '" << targetCan << "' to G4_WATER\n";
      OpticalProperties::DumpWaterMPT(canLV->GetMaterial(), canLV->GetName());

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

      if (opticsLoaded && canPV && opticsTables.wallSurface) {
        new G4LogicalBorderSurface("WaterToWorld", canPV,  worldPV, opticsTables.wallSurface);
        new G4LogicalBorderSurface("WorldToWater", worldPV, canPV,  opticsTables.wallSurface);
      }

      G4OpticalSurface* cathBorderSurface = nullptr;
      if (opticsLoaded && opticsTables.photocathodeSurface) {
        cathBorderSurface = new G4OpticalSurface("PhotocathodeWaterBoundary");
        cathBorderSurface->SetType(dielectric_dielectric);
        cathBorderSurface->SetModel(unified);
        cathBorderSurface->SetFinish(polished);

        if (!opticsTables.energyGrid.empty()) {
          auto* borderMPT = new G4MaterialPropertiesTable();
          std::vector<G4double> energyVec(opticsTables.energyGrid.begin(), opticsTables.energyGrid.end());
          std::vector<G4double> reflectivity(energyVec.size(), 0.0);

          if (auto* srcMPT = opticsTables.photocathodeSurface->GetMaterialPropertiesTable()) {
            if (auto* refVec = srcMPT->GetProperty("REFLECTIVITY")) {
              const size_t n = std::min(reflectivity.size(),
                                        static_cast<size_t>(refVec->GetVectorLength()));
              for (size_t i = 0; i < n; ++i) {
                reflectivity[i] = (*refVec)[i];
              }
            }
          }

          borderMPT->AddProperty("REFLECTIVITY",
                                 energyVec.data(),
                                 reflectivity.data(),
                                 energyVec.size());
          std::vector<G4double> zeroEff(energyVec.size(), 0.0);
          borderMPT->AddProperty("EFFICIENCY",
                                 energyVec.data(),
                                 zeroEff.data(),
                                 energyVec.size());
          cathBorderSurface->SetMaterialPropertiesTable(borderMPT);
        }
      }

      auto* cathMat = nist->FindOrBuildMaterial("G4_Al");
      if (!cathMat) {
        G4cout << "[WARN] Material 'G4_Al' not found; skipping PMT tiling.\n";
      } else {
        const G4double kPmtRadius = 0.10 * m;
        const G4double kPmtThick  = 0.8 * mm;
        const G4double defaultZPitch = 0.50 * m;
        const G4int   defaultNPhi   = 48;

        auto* pmtSol = new G4Tubs("PMT_cathode_tubs", 0., kPmtRadius, kPmtThick / 2., 0., 360 * deg);
        auto* pmtLog = new G4LogicalVolume(pmtSol,
                                           opticsTables.photocathodeMaterial ? opticsTables.photocathodeMaterial : cathMat,
                                           "PMT_cathode_log");
        if (opticsTables.photocathodeMaterial) {
          const size_t nGrid = !opticsTables.wavelength_nm.empty()
                                 ? opticsTables.wavelength_nm.size()
                                 : opticsTables.energyGrid.size();
          G4cout << "[PMT] PhotocathodeLV material="
                 << opticsTables.photocathodeMaterial->GetName()
                 << " with RINDEX(λ) set (N=" << nGrid << ")" << G4endl;
        }

        auto* existingSD = pmtLog->GetSensitiveDetector();
        PMTSD* pmtSD = dynamic_cast<PMTSD*>(existingSD);
        if (!pmtSD) {
          pmtSD = new PMTSD("PMTSD");
          G4SDManager::GetSDMpointer()->AddNewDetector(pmtSD);
          pmtLog->SetSensitiveDetector(pmtSD);
          G4cout << "[PMT] SD attached to PhotocathodeLV thickness="
                 << (kPmtThick / mm) << " mm" << G4endl;
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
        bool firstPmtMotherChecked = false;
        G4int overlapsChecked = 0;
        bool overlapsFound = false;

        auto placePmt = [&](G4RotationMatrix* rot, const G4ThreeVector& pos) -> G4VPhysicalVolume* {
          const G4int copyNo = pmtPlaced++;
          const bool shouldCheckOverlap = (fCheckOverlapsN > 0 && copyNo < fCheckOverlapsN);
          auto* pv = new G4PVPlacement(rot, pos, pmtLog, "PMT", canLV, false, copyNo, false);
          if (pv && !firstPmtMotherChecked) {
            auto* mother = pv->GetMotherLogical();
            const G4String motherName = mother ? mother->GetName() : "<null>";
            G4cout << "[CHK] pcath mother=" << motherName << G4endl;
            if (mother != canLV) {
              G4Exception("DetectorConstruction", "BadMother", FatalException,
                          "Photocathode must be direct child of water LV");
            }
            firstPmtMotherChecked = true;
          }
          if (cathBorderSurface && canPV && pv) {
            G4String surfName = "PhotocathodeSurface_" + std::to_string(copyNo + 1);
            new G4LogicalBorderSurface(surfName, canPV, pv, cathBorderSurface);
          }
          if (shouldCheckOverlap && pv) {
            ++overlapsChecked;
            if (pv->CheckOverlaps(0.0, 0.0, false)) {
              overlapsFound = true;
            }
          }
          return pv;
        };

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
                placePmt(rot, {x, y, z});
              }
            }
          }
          {
            G4double x = -halfX + 0.5 * cm;
            for (G4double z = -halfZ + kZPitch; z <= halfZ - kZPitch; z += kZPitch) {
              for (G4double y = -halfY + kYPitch; y <= halfY - kYPitch; y += kYPitch) {
                auto rot = new G4RotationMatrix();
                rot->rotateY(-90 * deg);
                placePmt(rot, {x, y, z});
              }
            }
          }
          {
            G4double y = halfY - 0.5 * cm;
            for (G4double z = -halfZ + kZPitch; z <= halfZ - kZPitch; z += kZPitch) {
              for (G4double x = -halfX + kXPitch; x <= halfX - kXPitch; x += kXPitch) {
                auto rot = new G4RotationMatrix();
                rot->rotateX(-90 * deg);
                placePmt(rot, {x, y, z});
              }
            }
          }
          {
            G4double y = -halfY + 0.5 * cm;
            for (G4double z = -halfZ + kZPitch; z <= halfZ - kZPitch; z += kZPitch) {
              for (G4double x = -halfX + kXPitch; x <= halfX - kXPitch; x += kXPitch) {
                auto rot = new G4RotationMatrix();
                rot->rotateX(90 * deg);
                placePmt(rot, {x, y, z});
              }
            }
          }
          const int sdAttached = (pmtLog->GetSensitiveDetector() != nullptr) ? 1 : 0;
          G4cout << "[PMT] placed=" << pmtPlaced << " rings=NA perRing=NA wall=1 endcaps=0" << G4endl;
          G4cout << "[SENS] SD attached=" << sdAttached << G4endl;
        } else if (auto* tubs = dynamic_cast<G4Tubs*>(canLV->GetSolid())) {
          const G4double rOuter   = tubs->GetOuterRadius();
          const G4double zHalf    = tubs->GetZHalfLength();
          const G4double wallGap  = 5.0 * mm;
          const G4double zMargin  = kPmtRadius + 5.0 * cm;
          G4double zPitch = read_env_double("FLNDR_PMT_ZPITCH_M", defaultZPitch / m) * m;
          if (zPitch <= 0.0) zPitch = defaultZPitch;
          const G4int nPhi = read_env_int("FLNDR_PMT_NPHI", defaultNPhi);
          const G4double innerRadius = std::max(rOuter - wallGap - kPmtRadius, kPmtRadius + wallGap);
          const G4double radius = innerRadius;

          int ringCount = 0;
          for (G4double z = -zHalf + zMargin; z <= zHalf - zMargin + 0.5 * zPitch; z += zPitch) {
            if (z > zHalf - zMargin) break;
            ++ringCount;
            for (int k = 0; k < nPhi; ++k) {
              const G4double phi = (2.0 * CLHEP::pi * k) / nPhi;
              const G4double x = radius * std::cos(phi);
              const G4double y = radius * std::sin(phi);
              auto rot = new G4RotationMatrix();
              rot->rotateY(90.0 * deg);
              rot->rotateZ(phi);
              placePmt(rot, {x, y, z});
            }
          }
          const int sdAttached = (pmtLog->GetSensitiveDetector() != nullptr) ? 1 : 0;
          G4cout << "[PMT] placed=" << pmtPlaced
                 << " rings=" << ringCount
                 << " perRing=" << nPhi
                 << " wall=1 endcaps=0" << G4endl;
          G4cout << "[SENS] SD attached=" << sdAttached << G4endl;
        }

        if (fCheckOverlapsN > 0) {
          G4cout << "[CHK] overlaps_checked=" << overlapsChecked
                 << " overlaps_found=" << (overlapsFound ? 1 : 0) << G4endl;
        }
      }
    }
  } else {
    G4cout << "[WARN] Logical volume '" << targetCan
           << "' not found. Skipping can material override.\n";
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

  if (opticsLoaded) {
    if (auto* surf = opticsTables.photocathodeSurface) {
      auto* mpt = surf->GetMaterialPropertiesTable();
      auto* eff = mpt ? mpt->GetProperty("EFFICIENCY") : nullptr;
      auto* ref = mpt ? mpt->GetProperty("REFLECTIVITY") : nullptr;
      auto firstValue = [](G4MaterialPropertyVector* vec) {
        if (!vec || vec->GetVectorLength() == 0) return 0.0;
        return (*vec)[0];
      };
      G4cout << "[SURF] photocathode type=" << SurfaceTypeName(surf->GetType())
             << " model=" << SurfaceModelName(surf->GetModel())
             << " finish=" << SurfaceFinishName(surf->GetFinish())
             << " EFF0=" << firstValue(eff)
             << " REF0=" << firstValue(ref) << G4endl;
    }
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

  auto* surfaceTable = G4LogicalBorderSurface::GetSurfaceTable();
  if (surfaceTable && !surfaceTable->empty()) {
    for (const auto& kv : *surfaceTable) {
      auto* surface = kv.second;
      if (!surface) continue;
      auto* v1 = surface->GetVolume1();
      auto* v2 = surface->GetVolume2();
      G4cout << "[SURF_TAB] " << surface->GetName()
             << " pv1=" << (v1 ? v1->GetName() : "<null>")
             << " pv2=" << (v2 ? v2->GetName() : "<null>") << G4endl;
    }
  } else {
    G4Exception("DetectorConstruction", "SurfaceTableEmpty", FatalException,
                "Expected at least one logical border surface.");
  }

  return worldPV;
}
