#include "PMTHit.hh"

#include "G4SystemOfUnits.hh"
#include "G4ios.hh"

#include <iomanip>

void PMTHit::Print() {
  G4cout << "[PMT Hit] id=" << pmt_id
         << " t=" << time / ns << " ns"
         << " npe=" << pe;
  if (wavelength_nm > 0.0) {
    G4cout << " lambda=" << wavelength_nm << " nm";
  }
  if (flags != 0) {
    G4cout << " flags=0x" << std::hex << flags << std::dec;
  }
  G4cout << G4endl;
}
