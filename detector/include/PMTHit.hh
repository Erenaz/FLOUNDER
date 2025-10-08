#pragma once

#include "G4THitsCollection.hh"
#include "G4VHit.hh"

class PMTHit : public G4VHit {
public:
  PMTHit() = default;
  PMTHit(G4int id, G4double t, G4double npe) : pmt_id(id), time(t), pe(npe) {}

  G4int pmt_id{-1};
  G4double time{0.0};
  G4double pe{0.0};

  void Print() override {}
};

using PMTHitsCollection = G4THitsCollection<PMTHit>;
