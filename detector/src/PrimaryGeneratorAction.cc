#include "PrimaryGeneratorAction.hh"

#include "RootrackerPrimaryGenerator.hh"

#include <G4GenericMessenger.hh>
#include <G4ParticleGun.hh>
#include <G4ParticleTable.hh>
#include <G4SystemOfUnits.hh>
#include <G4ThreeVector.hh>
#include <G4ios.hh>

PrimaryGeneratorAction::PrimaryGeneratorAction(const G4String& rootFile,
                                               G4double zShiftMM)
  : fRootFile(rootFile), fZshiftMM(zShiftMM) {
  // Default gun configuration (mu- @ 1 GeV forward)
  auto* muMinus = G4ParticleTable::GetParticleTable()->FindParticle("mu-");
  fGun = std::make_unique<G4ParticleGun>(1);
  if (muMinus) {
    fGun->SetParticleDefinition(muMinus);
  }
  fGun->SetParticleEnergy(1.0 * GeV);
  fGun->SetParticleMomentumDirection(G4ThreeVector(0., 0., 1.));

  fMessenger = std::make_unique<G4GenericMessenger>(this, "/fln/", "FLOUNDER controls");
  auto& cmd = fMessenger->DeclareMethod("genMode",
                                        &PrimaryGeneratorAction::SetGeneratorMode,
                                        "Select primary generator mode: rootracker or gun");
  cmd.SetParameterName("mode", false);
  cmd.SetCandidates("rootracker gun");
  cmd.SetDefaultValue("rootracker");
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() = default;

void PrimaryGeneratorAction::SetGeneratorMode(const G4String& mode) {
  if (mode != "rootracker" && mode != "gun") {
    G4Exception("PrimaryGeneratorAction::SetGeneratorMode", "BadMode",
                JustWarning, ("Unknown generator mode '" + mode + "'").c_str());
    return;
  }
  if (mode == fMode) return;
  fMode = mode;
  fAnnounced = false;  // re-announce on next event
}

RootrackerPrimaryGenerator* PrimaryGeneratorAction::EnsureRootracker() {
  if (!fRootracker) {
    if (fRootFile.empty()) {
      G4Exception("PrimaryGeneratorAction::EnsureRootracker", "MissingRootFile",
                  FatalException, "G4_ROOTRACKER not set (required for rootracker mode).");
    }
    fRootracker = std::make_unique<RootrackerPrimaryGenerator>(fRootFile, fZshiftMM);
  }
  return fRootracker.get();
}

void PrimaryGeneratorAction::AnnounceModeOnce() {
  if (!fAnnounced) {
    G4cout << "[CFG] genMode=" << fMode << G4endl;
    fAnnounced = true;
  }
}

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* event) {
  AnnounceModeOnce();

  if (fMode == "gun") {
    fGun->GeneratePrimaryVertex(event);
  } else {
    EnsureRootracker()->GeneratePrimaries(event);
  }
}
