#include "PhotonCountActions.hh"
#include "G4Event.hh"
#include "G4OpticalPhoton.hh"
#include "G4Track.hh"
#include <iostream>

void PhotonCountEventAction::EndOfEventAction(const G4Event*) {
  std::cout << "[Optics] Event optical photons created: " << count_ << std::endl;
}

G4ClassificationOfNewTrack
PhotonCountStackingAction::ClassifyNewTrack(const G4Track* track) {
  if (track->GetDefinition() == G4OpticalPhoton::Definition()) {
    if (evt_) evt_->Inc();
  }
  return fUrgent;
}
