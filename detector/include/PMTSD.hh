#pragma once

#include "G4VSensitiveDetector.hh"
#include "PMTHit.hh"

class PMTSD : public G4VSensitiveDetector {
public:
  explicit PMTSD(const G4String& name);

  void Initialize(G4HCofThisEvent* hce) override;
  G4bool ProcessHits(G4Step* step, G4TouchableHistory*) override;
  void EndOfEvent(G4HCofThisEvent* hce) override;

private:
  PMTHitsCollection* hits_{nullptr};
  G4int hc_id_{-1};
  G4int totalHits_{0};
};
