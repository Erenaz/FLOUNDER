#pragma once
#include "G4UserEventAction.hh"
#include "G4UserStackingAction.hh"

class PhotonCountEventAction : public G4UserEventAction {
public:
  PhotonCountEventAction() : count_(0) {}
  void BeginOfEventAction(const G4Event*) override { count_ = 0; }
  void EndOfEventAction(const G4Event*) override;
  void Inc() { ++count_; }
private:
  unsigned long long count_;
};

class PhotonCountStackingAction : public G4UserStackingAction {
public:
  explicit PhotonCountStackingAction(PhotonCountEventAction* evt) : evt_(evt) {}
  G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track* track) override;
private:
  PhotonCountEventAction* evt_;
};
