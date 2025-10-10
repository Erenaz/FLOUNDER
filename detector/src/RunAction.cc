#include "RunAction.hh"
#include "PhotonCountActions.hh"
#include "RunManifest.hh"

#include <G4Run.hh>
#include <G4ios.hh>

RunAction::RunAction() = default;

void RunAction::BeginOfRunAction(const G4Run*) {
  PhotonCountEventAction::ResetTotal();
  const auto& manifest = GetRunManifest();
  G4cout << "[Manifest] profile=" << manifest.profile
         << " macro=" << manifest.macro
         << " optics=" << manifest.opticsPath
         << " pmt=" << (manifest.pmtPath.empty() ? "<none>" : manifest.pmtPath)
         << " git=" << manifest.gitSHA
         << G4endl;
}

void RunAction::EndOfRunAction(const G4Run*) {
  G4cout << "[Optics] total_optical_photons="
         << PhotonCountEventAction::GetTotal() << G4endl;
  FlushManifestToOutputs();
}
