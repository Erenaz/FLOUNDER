#pragma once

#include <G4VUserPrimaryGeneratorAction.hh>
#include <G4ThreeVector.hh>
#include <G4String.hh>
#include <memory>

class G4Event;
class G4ParticleGun;
class G4GenericMessenger;

class RootrackerPrimaryGenerator;

class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction {
public:
  PrimaryGeneratorAction(const G4String& rootFile, G4double zShiftMM);
  ~PrimaryGeneratorAction() override;

  void GeneratePrimaries(G4Event* event) override;

  void SetGeneratorMode(const G4String& mode);
  const G4String& GetGeneratorMode() const { return fMode; }

private:
  RootrackerPrimaryGenerator* EnsureRootracker();
  void AnnounceModeOnce();
  void AimAtPMTCommand(const G4String& args);
  void AimAtPMT(int id, double offset_mm, double energy_eV, int count);
  void SetPhotonGunCount(int count);

  G4String fRootFile;
  G4double fZshiftMM = 0.0;

  std::unique_ptr<RootrackerPrimaryGenerator> fRootracker;
  std::unique_ptr<G4ParticleGun>              fGun;

  G4String fMode = "rootracker";
  bool     fAnnounced = false;
  int      fGunPhotonCount = 1;

  std::unique_ptr<G4GenericMessenger> fMessenger;
};
