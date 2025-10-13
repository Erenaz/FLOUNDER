#pragma once

#include <mutex>
#include <vector>

#include <G4ThreeVector.hh>

struct PMTRecord {
  int id = -1;
  G4ThreeVector position; // center in detector coordinates (mm)
  G4ThreeVector normal;   // unit vector pointing into water
};

class GeometryRegistry {
public:
  static GeometryRegistry& Instance();

  void ClearPMTs();
  void RegisterPMT(int id, const G4ThreeVector& position, const G4ThreeVector& normal);
  bool GetPMT(int id, PMTRecord& out) const;
  const std::vector<PMTRecord>& GetPMTs() const { return fPMTs; }

private:
  GeometryRegistry() = default;

  mutable std::mutex fMutex;
  std::vector<PMTRecord> fPMTs;
};
