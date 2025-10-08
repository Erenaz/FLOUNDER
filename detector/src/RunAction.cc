#include "RunAction.hh"
#include "PhotonCountActions.hh"

#include <G4Run.hh>
#include <G4ios.hh>

RunAction::RunAction() = default;

void RunAction::BeginOfRunAction(const G4Run*) {
  PhotonCountEventAction::ResetTotal();
}

void RunAction::EndOfRunAction(const G4Run*) {
  G4cout << "[Optics] total_optical_photons="
         << PhotonCountEventAction::GetTotal() << G4endl;
}
