#include "OpticalProperties.hh"
#include "PhysicsList.hh"

#include <G4Box.hh>
#include <G4Event.hh>
#include <G4LogicalVolume.hh>
#include <G4NistManager.hh>
#include <G4PVPlacement.hh>
#include <G4OpticalPhoton.hh>
#include <G4ParticleGun.hh>
#include <G4ParticleTable.hh>
#include <G4RunManagerFactory.hh>
#include <G4RunManager.hh>
#include <G4SystemOfUnits.hh>
#include <G4Track.hh>
#include <G4UserTrackingAction.hh>
#include <G4VUserActionInitialization.hh>
#include <G4VUserDetectorConstruction.hh>
#include <G4VUserPrimaryGeneratorAction.hh>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

namespace {

class TestDetectorConstruction : public G4VUserDetectorConstruction {
public:
  G4VPhysicalVolume* Construct() override {
    auto* nist = G4NistManager::Instance();
    auto* vacuum = nist->FindOrBuildMaterial("G4_Galactic");
    auto* water  = nist->FindOrBuildMaterial("G4_WATER");

    const std::string opticsPath = std::string(FLNDR_SOURCE_DIR) + "/detector/config/optics.yaml";
    OpticalPropertiesResult optics = OpticalProperties::LoadFromYaml(opticsPath);
    water->SetMaterialPropertiesTable(optics.waterMPT);
    OpticalProperties::AttachVacuumRindex(vacuum, optics.energyGrid);

    auto* solidWorld = new G4Box("World", 6.0 * m, 6.0 * m, 6.0 * m);
    auto* logicWorld = new G4LogicalVolume(solidWorld, vacuum, "WorldLV");
    auto* physWorld  = new G4PVPlacement(nullptr, {}, logicWorld, "World", nullptr, false, 0);

    auto* solidWater = new G4Box("WaterSlab", 1.0 * m, 1.0 * m, 5.0 * m); // 10 m along z
    auto* logicWater = new G4LogicalVolume(solidWater, water, "WaterLV");
    new G4PVPlacement(nullptr, {}, logicWater, "Water", logicWorld, false, 0);

    return physWorld;
  }
};

class TestPrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction {
public:
  TestPrimaryGeneratorAction() {
    gun_ = std::make_unique<G4ParticleGun>(1);
    auto* table = G4ParticleTable::GetParticleTable();
    auto* muMinus = table->FindParticle("mu-");
    gun_->SetParticleDefinition(muMinus);
    gun_->SetParticleEnergy(50.0 * GeV);
    gun_->SetParticlePosition(G4ThreeVector(0., 0., -5.0 * m));
    gun_->SetParticleMomentumDirection(G4ThreeVector(0., 0., 1.0));
  }

  void GeneratePrimaries(G4Event* event) override {
    gun_->GeneratePrimaryVertex(event);
  }

private:
  std::unique_ptr<G4ParticleGun> gun_;
};

class PhotonCountTrackingAction : public G4UserTrackingAction {
public:
  void PreUserTrackingAction(const G4Track* track) override {
    if (track->GetDefinition() == G4OpticalPhoton::Definition()) {
      ++count_;
    }
  }

  int count() const { return count_; }

private:
  int count_ = 0;
};

class TestActionInitialization : public G4VUserActionInitialization {
public:
  explicit TestActionInitialization(PhotonCountTrackingAction** counterRef)
    : counterRef_(counterRef) {}

  void Build() const override {
    SetUserAction(new TestPrimaryGeneratorAction());
    auto* tracker = new PhotonCountTrackingAction();
    if (counterRef_) {
      *counterRef_ = tracker;
    }
    SetUserAction(tracker);
  }

private:
  PhotonCountTrackingAction** counterRef_;
};

} // namespace

int main() {
  auto* runManager = G4RunManagerFactory::CreateRunManager(G4RunManagerType::Default);

  runManager->SetUserInitialization(new TestDetectorConstruction());

  OpticalProcessConfig optCfg;
  optCfg.enableCerenkov = true;
  optCfg.enableAbsorption = true;
  optCfg.enableRayleigh = true;
  optCfg.enableMie = false;
  optCfg.enableBoundary = true;
  optCfg.maxPhotonsPerStep = 300;
  optCfg.maxBetaChangePerStep = 10.0;
  runManager->SetUserInitialization(new PhysicsList(optCfg));

  PhotonCountTrackingAction* counter = nullptr;
  runManager->SetUserInitialization(new TestActionInitialization(&counter));

  runManager->Initialize();
  runManager->BeamOn(1);

  int photons = counter ? counter->count() : 0;
  std::cout << "[light_yield] photons=" << photons << std::endl;

  delete runManager;

  if (!counter) {
    std::cerr << "[light_yield] tracking counter not set.\n";
    return 1;
  }

  const double expected = 2.0e5;
  const double tolerance = expected * 0.30;
  const double minAccept = expected - tolerance;
  const double maxAccept = expected + tolerance;

  if (photons < minAccept || photons > maxAccept) {
    std::cerr << "[light_yield] FAIL: expected within [" << minAccept
              << ", " << maxAccept << "], got " << photons << std::endl;
    return 1;
  }

  std::cout << "[light_yield] PASS: within tolerance." << std::endl;
  return 0;
}
