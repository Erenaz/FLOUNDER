#pragma once
#include "G4UserEventAction.hh"
#include "G4UserStackingAction.hh"

class PhotonCountEventAction : public G4UserEventAction {
public:
  PhotonCountEventAction() : count_(0) {}
  void BeginOfEventAction(const G4Event*) override { count_ = 0; }
  void EndOfEventAction(const G4Event*) override;
  void Inc();
  static void ResetTotal();
  static unsigned long long GetTotal();
private:
  unsigned long long count_;
  static unsigned long long total_;
};

class PhotonCountStackingAction : public G4UserStackingAction {
public:
  explicit PhotonCountStackingAction(PhotonCountEventAction* evt) : evt_(evt) {}
  G4ClassificationOfNewTrack ClassifyNewTrack(const G4Track* track) override;
private:
  PhotonCountEventAction* evt_;
};
