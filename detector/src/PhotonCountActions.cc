#include "PhotonCountActions.hh"
#include "RunManifest.hh"
#include "G4Event.hh"
#include "G4OpticalPhoton.hh"
#include "G4Track.hh"
#include <G4ios.hh>

unsigned long long PhotonCountEventAction::total_ = 0;

void PhotonCountEventAction::EndOfEventAction(const G4Event*) {
  const auto& cfg = GetRunManifest();
  if (!cfg.quiet && cfg.opticalVerboseLevel > 0) {
    G4cout << "[Optics] Event optical photons created: " << count_ << G4endl;
  }
}

void PhotonCountEventAction::Inc() {
  ++count_;
  ++total_;
}

void PhotonCountEventAction::ResetTotal() {
  total_ = 0;
}

unsigned long long PhotonCountEventAction::GetTotal() {
  return total_;
}

G4ClassificationOfNewTrack
PhotonCountStackingAction::ClassifyNewTrack(const G4Track* track) {
  if (track->GetDefinition() == G4OpticalPhoton::Definition()) {
    if (evt_) evt_->Inc();
  }
  return fUrgent;
}
