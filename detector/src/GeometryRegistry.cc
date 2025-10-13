#include "GeometryRegistry.hh"

#include <algorithm>

GeometryRegistry& GeometryRegistry::Instance() {
  static GeometryRegistry instance;
  return instance;
}

void GeometryRegistry::ClearPMTs() {
  std::lock_guard<std::mutex> lock(fMutex);
  fPMTs.clear();
}

void GeometryRegistry::RegisterPMT(int id, const G4ThreeVector& position, const G4ThreeVector& normal) {
  std::lock_guard<std::mutex> lock(fMutex);
  auto it = std::find_if(fPMTs.begin(), fPMTs.end(), [id](const PMTRecord& rec) { return rec.id == id; });
  PMTRecord rec;
  rec.id = id;
  rec.position = position;
  rec.normal = normal;
  if (it != fPMTs.end()) {
    *it = rec;
  } else {
    fPMTs.push_back(rec);
  }
}

bool GeometryRegistry::GetPMT(int id, PMTRecord& out) const {
  std::lock_guard<std::mutex> lock(fMutex);
  auto it = std::find_if(fPMTs.begin(), fPMTs.end(), [id](const PMTRecord& rec) { return rec.id == id; });
  if (it == fPMTs.end()) return false;
  out = *it;
  return true;
}
