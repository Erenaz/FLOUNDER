#pragma once
#include <G4VUserPrimaryGeneratorAction.hh>
#include <G4ThreeVector.hh>
#include <G4GenericMessenger.hh>
#include <memory>
#include <G4String.hh> 
class G4Event;
class TFile; class TTree;

class RootrackerPrimaryGenerator : public G4VUserPrimaryGeneratorAction {
public:
  RootrackerPrimaryGenerator(const G4String& fname, G4double zShiftMM=0.0);
  ~RootrackerPrimaryGenerator() override;
  void GeneratePrimaries(G4Event* event) override;

  // Expose controls at runtime: /rootracker/...
  void SetEventIndex(long long i) { fNextIndex = i; }
  long long GetEventIndex() const { return fNextIndex; }
  void SetZShiftMM(double dz) { fZShiftMM = dz; }

private:
  bool LoadTree();
  bool LoadEntry(long long i);

  // ROOT handles
  std::unique_ptr<TFile> fFile;
  TTree* fTree = nullptr;

  // Branch pointers (typical ROOTRACKER names)
  static constexpr int kMAX = 10000;
  double EvtVtx[4]{};                 // (x[m], y[m], z[m], t[s])
  int    StdHepN = 0;
  int    StdHepPdg[kMAX];
  int    StdHepStatus[kMAX];
  double StdHepP4[kMAX][4];          // (px,py,pz,E) in GeV

  // config
  G4String fFileName;
  long long fNextIndex = 0;
  double fZShiftMM = 0.0;            // map gSeaGen z(mm) -> GDML
  std::unique_ptr<G4GenericMessenger> fMsg;
};

