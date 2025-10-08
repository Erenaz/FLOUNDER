#include "OpticalInit.hh"
#include "OpticalPropertiesLoader.hh"

#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>

#include "G4LogicalBorderSurface.hh"
#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4NistManager.hh"
#include "G4OpticalSurface.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4SystemOfUnits.hh"
#include "G4ios.hh"

using std::cout; using std::endl;

namespace {
  PMTSummary gPMTSummary;
  G4OpticalSurface* gPhotocathodeSurface = nullptr;
}

namespace OpticalInit {

  static std::string fmt_rng(double a, double b, int prec, const char* unit="") {
    std::ostringstream os; os.setf(std::ios::fixed); os << std::setprecision(prec)
        << "[" << a << "," << b << "]" << unit;
    return os.str();
  }

  // convenience overload (defaults to 1 decimal place)
  // static std::string fmt_rng(double a, double b, const char* unit) {
  //   return fmt_rng(a, b, 1, unit);
  // }

  bool ConfigureOptics(const std::string& water_csv,
                       const std::string& pmt_qe_csv,
                       G4VPhysicalVolume* worldPV,
                       G4VPhysicalVolume* canPV) {
    try {
      // 1) Build and attach water optical table
      auto* water = G4Material::GetMaterial("G4_WATER", true);
      if (!water) { std::cerr << "G4_WATER not found.\n"; return false; }

      WaterOpticsSummary ws;
      auto* mpt_water = OpticalPropertiesLoader::BuildWaterMPTFromCSV(water_csv, ws);
      water->SetMaterialPropertiesTable(mpt_water);
      {
        const auto hasN = mpt_water && mpt_water->GetProperty("RINDEX");
        const auto hasA = mpt_water && mpt_water->GetProperty("ABSLENGTH");
        const auto hasR = mpt_water && mpt_water->GetProperty("RAYLEIGH");
        G4cout << "[OPT] water MPT set: n=" << (hasN ? 1 : 0)
               << " abs=" << (hasA ? 1 : 0)
               << " ray=" << (hasR ? 1 : 0) << G4endl;
      }
      {
        auto* mpt = water->GetMaterialPropertiesTable();
        auto* rindex   = mpt ? mpt->GetProperty("RINDEX")    : nullptr;
        auto* absl     = mpt ? mpt->GetProperty("ABSLENGTH") : nullptr;
        auto* rayleigh = mpt ? mpt->GetProperty("RAYLEIGH")  : nullptr;
        if (!rindex || !absl || !rayleigh) {
          G4Exception("ConfigureOptics","OpticsMissing",FatalException,
                      "Water MPT missing one or more properties (RINDEX/ABSLENGTH/RAYLEIGH).");
        }
        const auto nR = rindex->GetVectorLength();
        if (nR != absl->GetVectorLength() || nR != rayleigh->GetVectorLength()) {
          G4Exception("ConfigureOptics","OpticsTableSize",FatalException,
                      "Water MPT property vectors have unequal lengths.");
        }
      }

      // 2) Attach RINDEX=1.0 to Galactic over the SAME energy grid (quiet boundaries)
      auto* vacuum = G4Material::GetMaterial("G4_Galactic");
      OpticalPropertiesLoader::AttachVacuumRindex(vacuum, ws.energy_grid);

      // 3) Add a generic optical boundary on the can wall (dielectric-dielectric, UNIFIED model)
      if (worldPV && canPV) {
        auto* surf = new G4OpticalSurface("WaterBoundary",
                                          unified, ground, dielectric_dielectric, 1.0);
        // Optional roughness tweak (sigma-alpha in radians) if desired:
        // surf->SetSigmaAlpha(0.1);

        // Define border surfaces both ways (explicit):
        new G4LogicalBorderSurface("WaterToWorld", canPV,  worldPV, surf);
        new G4LogicalBorderSurface("WorldToWater", worldPV, canPV,  surf);
      }

      // 4) Load PMT QE and compute ⟨QE⟩ in 400–450 nm (for logging)
      PMTSummary ps = OpticalPropertiesLoader::LoadPMTQE(pmt_qe_csv, 400.0, 450.0);
      gPMTSummary = ps;

      if (!gPhotocathodeSurface) {
        gPhotocathodeSurface = new G4OpticalSurface("PhotocathodeSurface",
                                                    unified, polished, dielectric_metal);
      }
      if (!ps.energy.empty()) {
        std::vector<G4double> reflectivity(ps.efficiency.size(), 0.0);
        for (size_t i = 0; i < ps.efficiency.size(); ++i) {
          reflectivity[i] = std::max(0.0, 1.0 - ps.efficiency[i]);
        }
        auto* cathodeMPT = new G4MaterialPropertiesTable();
        cathodeMPT->AddProperty("EFFICIENCY",   ps.energy, ps.efficiency);
        cathodeMPT->AddProperty("REFLECTIVITY", ps.energy, reflectivity);
        gPhotocathodeSurface->SetMaterialPropertiesTable(cathodeMPT);
      }

      // 5) Print compact DoD-style summaries
      cout << "[Optics] Water optics: λ=" << fmt_rng(ws.lambda_min_nm, ws.lambda_max_nm, 1, " nm")
           << " (N=" << ws.npoints << "); n=" << fmt_rng(ws.n_min, ws.n_max, 4)
           << "; L_abs="  << fmt_rng(ws.labs_min_m, ws.labs_max_m, 1, " m")
           << "; L_scat=" << fmt_rng(ws.lsca_min_m, ws.lsca_max_m, 1, " m")
           << endl;


      cout << "[Optics] PMT QE: λ=" << fmt_rng(ps.lambda_min_nm, ps.lambda_max_nm, 1, " nm")
           << " (N=" << ps.npoints << "); <QE>_400–450nm = "
           << std::fixed << std::setprecision(1) << (ps.mean_qe_400_450*100.0) << " %"
           << endl;

      return true;

    } catch (const std::exception& ex) {
      std::cerr << "[Optics] ERROR: " << ex.what() << "\n";
      return false;
    }
  }

  const PMTSummary& GetPMTSummary() { return gPMTSummary; }
  G4OpticalSurface* GetPhotocathodeSurface() { return gPhotocathodeSurface; }
}
