#include "RootrackerPrimaryGenerator.hh"
#include <G4Event.hh>
#include <G4PrimaryVertex.hh>
#include <G4PrimaryParticle.hh>
#include <G4ParticleTable.hh>
#include <G4SystemOfUnits.hh>
#include <G4UnitsTable.hh>
#include <G4ThreeVector.hh>
#include <G4ios.hh>
#include "PhotonBudget.hh"
#include <G4RunManager.hh>

#include <TFile.h>
#include <TTree.h>
#include <TLeaf.h>
#include <TKey.h>
#include <cmath>
#include <stdexcept>

static TTree* FindTreeByGuess(TFile* f) {
  if (!f) return nullptr;
  // common name
  if (auto* t = dynamic_cast<TTree*>(f->Get("gRooTracker"))) return t;
  // otherwise pick the first TTree in the file
  TIter nextkey(f->GetListOfKeys());
  while (auto* key = (TKey*)nextkey()) {
    if (std::string(key->GetClassName()) == "TTree")
      return dynamic_cast<TTree*>(key->ReadObj());
  }
  return nullptr;
}

RootrackerPrimaryGenerator::RootrackerPrimaryGenerator(const G4String& fname, G4double zShiftMM)
: fFileName(fname), fZShiftMM(zShiftMM)
{
  if (!LoadTree())
    G4Exception("RootrackerPrimaryGenerator","NoTree",FatalException,
                ("Failed to open ROOTRACKER tree from "+std::string(fname)).c_str());

  // UI: /rootracker/*
  fMsg = std::make_unique<G4GenericMessenger>(this,"/rootracker/","Rootracker controls");
  fMsg->DeclareProperty("eventIndex", fNextIndex, "Set next entry index (0-based).");
  fMsg->DeclareProperty("zShiftMM",   fZShiftMM,  "Additive z shift [mm] to map CAN->GDML.");
}

RootrackerPrimaryGenerator::~RootrackerPrimaryGenerator() = default;

bool RootrackerPrimaryGenerator::LoadTree() {
  fFile.reset(TFile::Open(fFileName.c_str(),"READ"));
  if (!fFile || fFile->IsZombie()) return false;
  fTree = FindTreeByGuess(fFile.get());
  if (!fTree) return false;

  // Attach branches if present
  fTree->SetBranchStatus("*",0);
  auto enable = [&](const char* b){
    if (fTree->GetBranch(b)) { fTree->SetBranchStatus(b,1); return true; }
    return false;
  };
  enable("EvtVtx");      fTree->SetBranchAddress("EvtVtx",     EvtVtx);
  enable("StdHepN");     fTree->SetBranchAddress("StdHepN",    &StdHepN);
  enable("StdHepPdg");   fTree->SetBranchAddress("StdHepPdg",  StdHepPdg);
  if (enable("StdHepStatus"))
    fTree->SetBranchAddress("StdHepStatus", StdHepStatus);
  if (enable("StdHepP4"))
    fTree->SetBranchAddress("StdHepP4",     StdHepP4);
  else
    G4Exception("RootrackerPrimaryGenerator","NoP4",JustWarning,
                "StdHepP4 not found; momentumless primaries would be useless.");

  G4cout << "[Rootracker] Opened " << fFileName << " with " << fTree->GetEntries() << " entries\n";
  return true;
}

bool RootrackerPrimaryGenerator::LoadEntry(long long i) {
  if (!fTree) return false;
  if (i < 0 || i >= fTree->GetEntries()) return false;
  fTree->GetEntry(i);
  return true;
}

void RootrackerPrimaryGenerator::GeneratePrimaries(G4Event* event) {
  if (!LoadEntry(fNextIndex)) {
    // Graceful end-of-tree: stop the run without throwing a fatal
    G4Exception("RootrackerPrimaryGenerator","EndOfTree",JustWarning,
                "No more ROOT entries; aborting run cleanly.");
    auto* rm = G4RunManager::GetRunManager();
    if (rm) rm->AbortRun(true);
    return;
  }

  // Choose the outgoing charged lepton (prefer mu±, status==1 if available)
  int idx = -1;
  for (int j=0; j<StdHepN; ++j) {
    int pdg = StdHepPdg[j];
    if (pdg==13 || pdg==-13) {
      if (fTree->GetBranch("StdHepStatus")) {
        if (StdHepStatus[j]==1) { idx = j; break; }
      } else { idx = j; break; }
    }
  }
  if (idx<0) {
    // fallback: pick the highest-momentum charged final state
    double bestp2 = -1;
    for (int j=0; j<StdHepN; ++j) {
      if (fTree->GetBranch("StdHepStatus") && StdHepStatus[j]!=1) continue;
      int pdg = StdHepPdg[j];
      if (pdg==22 || pdg==12 || pdg==-12 || pdg==14 || pdg==-14 || pdg==16 || pdg==-16) continue; // skip γ, ν
      double px = StdHepP4[j][0], py=StdHepP4[j][1], pz=StdHepP4[j][2];
      double p2 = px*px+py*py+pz*pz;
      if (p2>bestp2) { bestp2=p2; idx=j; }
    }
  }
  if (idx<0)
    G4Exception("RootrackerPrimaryGenerator","NoLepton",FatalException,"No suitable final state found.");

  // Units: GeV→MeV; m→mm; s→ns
  const double px = StdHepP4[idx][0]*1000.0;
  const double py = StdHepP4[idx][1]*1000.0;
  const double pz = StdHepP4[idx][2]*1000.0;
  const double E  = StdHepP4[idx][3]*1000.0;

  const double vx = EvtVtx[0]*1000.0;
  const double vy = EvtVtx[1]*1000.0;
  const double vz = EvtVtx[2]*1000.0 + fZShiftMM;  // shift maps CAN→GDML
  const double tn = EvtVtx[3]*1.0e9;

  // G4 primary
  auto* vtx = new G4PrimaryVertex(G4ThreeVector(vx*mm, vy*mm, vz*mm), tn*ns);
  const G4ThreeVector x0(vtx->GetX0(), vtx->GetY0(), vtx->GetZ0());
  const double t0_ns = vtx->GetT0() / ns;   // convert Geant4 internal time to [ns] scalar
  PrimaryInfo::Set(x0, t0_ns);


  // PDG → particle
  auto* ptable = G4ParticleTable::GetParticleTable();
  auto* pdef = ptable->FindParticle(StdHepPdg[idx]);
  if (!pdef) {
    G4Exception("RootrackerPrimaryGenerator","UnknownPDG",JustWarning,"PDG not in table; forcing mu-");
    pdef = ptable->FindParticle(13);
  }
  auto* prim = new G4PrimaryParticle(pdef, px*MeV, py*MeV, pz*MeV);
  prim->SetTotalEnergy(E*MeV);

  vtx->SetPrimary(prim);
  event->AddPrimaryVertex(vtx);

  // Log for hand-checks
  const double pmod = std::sqrt(px*px+py*py+pz*pz);
  const double ux = px/pmod, uy = py/pmod, uz = pz/pmod;
  G4cout << "[Rootracker] evt=" << fNextIndex
         << " vtx(mm)=(" << vx << "," << vy << "," << vz << "), t(ns)=" << tn
         << " p(MeV)=(" << px << "," << py << "," << pz << "), |p|=" << pmod
         << " u=(" << ux << "," << uy << "," << uz << ")\n";

  // advance
  ++fNextIndex;
}
