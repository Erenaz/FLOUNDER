#include "PhysicsList.hh"

#include <string>

#include <G4OpticalParameters.hh>
#include <G4OpticalPhysics.hh>
#include <G4OpticalPhoton.hh>
#include <G4OpBoundaryProcess.hh>
#include <G4ProcessManager.hh>

#include "RunManifest.hh"
#include <G4SystemOfUnits.hh>
#include <G4ios.hh>

#include <algorithm>

namespace {

std::string OnOff(bool value) {
  return value ? "on" : "off";
}

} // namespace

PhysicsList::PhysicsList(const OpticalProcessConfig& cfg)
  : FTFP_BERT(), fConfig(cfg) {
  auto* optical = new G4OpticalPhysics();
  RegisterPhysics(optical);

  // Mirror configuration onto shared optical parameters singleton (used by processes at runtime).
  auto* params = G4OpticalParameters::Instance();
  params->SetCerenkovMaxPhotonsPerStep(fConfig.maxPhotonsPerStep);
  params->SetCerenkovMaxBetaChange(fConfig.maxBetaChangePerStep);
  params->SetCerenkovTrackSecondariesFirst(true);
  params->SetProcessActivation("Cerenkov",   fConfig.enableCerenkov);
  params->SetProcessActivation("Absorption", fConfig.enableAbsorption);
  params->SetProcessActivation("Rayleigh",   fConfig.enableRayleigh);
  params->SetProcessActivation("MieHG",      fConfig.enableMie);
  params->SetProcessActivation("Boundary",   fConfig.enableBoundary);

  G4cout << "[OPT] Optical physics configured: "
         << "Cerenkov=" << OnOff(fConfig.enableCerenkov)
         << " Abs=" << OnOff(fConfig.enableAbsorption)
         << " Rayleigh=" << OnOff(fConfig.enableRayleigh)
         << " Mie=" << OnOff(fConfig.enableMie)
         << " Boundary=" << OnOff(fConfig.enableBoundary)
         << " maxPhotons=" << fConfig.maxPhotonsPerStep
         << " maxBetaΔ=" << fConfig.maxBetaChangePerStep
         << G4endl;
}

void PhysicsList::ConstructProcess() {
  FTFP_BERT::ConstructProcess();

  auto* procMgr = G4OpticalPhoton::OpticalPhotonDefinition()->GetProcessManager();
  if (!procMgr) return;
  auto* processList = procMgr->GetProcessList();
  if (!processList) return;
  const int verboseLevel = std::clamp(GetRunManifest().opticalVerboseLevel, 0, 2);
  const size_t n = processList->size();
  for (size_t i = 0; i < n; ++i) {
    auto* process = (*processList)[i];
    if (auto* boundary = dynamic_cast<G4OpBoundaryProcess*>(process)) {
      boundary->SetVerboseLevel(verboseLevel);
    }
  }
}
