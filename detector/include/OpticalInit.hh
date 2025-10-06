#pragma once
#include <string>

class G4VPhysicalVolume;

namespace OpticalInit {
  // Attach water/ vacuum properties and make a border surface (water ↔ world).
  // Returns true on success and prints a one-line DoD-style summary to stdout.
  bool ConfigureOptics(const std::string& water_csv,
                       const std::string& pmt_qe_csv,
                       G4VPhysicalVolume* worldPV,
                       G4VPhysicalVolume* canPV);
}
