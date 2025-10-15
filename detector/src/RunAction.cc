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
  G4cout << "[Manifest] quiet=" << (manifest.quiet ? "on" : "off")
         << " opt_verbose=" << manifest.opticalVerboseLevel
         << " summary_every=" << manifest.summaryEvery
         << " digitizer_out=" << (manifest.digitizerOutput.empty() ? "<none>" : manifest.digitizerOutput)
         << G4endl;
}

void RunAction::EndOfRunAction(const G4Run*) {
  const auto& manifest = GetRunManifest();
  if (!manifest.quiet && manifest.opticalVerboseLevel > 0) {
    G4cout << "[Optics] total_optical_photons="
           << PhotonCountEventAction::GetTotal() << G4endl;
  }
  FlushManifestToOutputs();
}
