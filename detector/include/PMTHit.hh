#pragma once

#include "G4THitsCollection.hh"
#include "G4VHit.hh"

class PMTHit : public G4VHit {
public:
  PMTHit() = default;
  PMTHit(G4int id, G4double t, G4double npe,
         G4double lambda_nm = 0.0, G4int hitFlags = 0)
    : pmt_id(id),
      time(t),
      pe(npe),
      wavelength_nm(lambda_nm),
      flags(hitFlags) {}

  G4int    pmt_id{-1};
  G4double time{0.0};
  G4double pe{0.0};
  G4double wavelength_nm{0.0};
  G4int    flags{0};

  void Print() override;
};

using PMTHitsCollection = G4THitsCollection<PMTHit>;
