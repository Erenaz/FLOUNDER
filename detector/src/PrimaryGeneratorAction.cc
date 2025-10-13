#include "PrimaryGeneratorAction.hh"

#include "GeometryRegistry.hh"
#include "RootrackerPrimaryGenerator.hh"

#include <G4GenericMessenger.hh>
#include <G4OpticalPhoton.hh>
#include <G4ParticleGun.hh>
#include <G4ParticleTable.hh>
#include <G4SystemOfUnits.hh>
#include <G4ThreeVector.hh>
#include <G4ios.hh>

#include <cmath>
#include <CLHEP/Units/PhysicalConstants.h>
#include <iomanip>
#include <sstream>
#include <vector>

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

  fMessenger->DeclareMethod("aimAtPMT",
                            &PrimaryGeneratorAction::AimAtPMTCommand,
                            "Aim optical gun at PMT: /fln/aimAtPMT <id> [offset_mm] [energy_eV]");
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

void PrimaryGeneratorAction::AimAtPMTCommand(const G4String& args) {
  std::istringstream iss(args);
  int id = -1;
  std::vector<double> values;
  double value = 0.0;

  if (!(iss >> id)) {
    G4cout << "[PHOTON_GUN] ERROR: /fln/aimAtPMT requires a PMT id." << G4endl;
    return;
  }
  while (iss >> value) {
    values.push_back(value);
  }
  double offset = values.size() > 0 ? values[0] : 50.0;
  double energy = values.size() > 1 ? values[1] : 3.0;
  AimAtPMT(id, offset, energy);
}

void PrimaryGeneratorAction::AimAtPMT(int id, double offset_mm, double energy_eV) {
  PMTRecord rec;
  if (!GeometryRegistry::Instance().GetPMT(id, rec)) {
    G4cout << "[PHOTON_GUN] ERROR: PMT id=" << id << " not found." << G4endl;
    return;
  }

  if (offset_mm <= 0.0) offset_mm = 50.0;
  if (energy_eV <= 0.0) energy_eV = 3.0;

  G4ThreeVector normal = rec.normal;
  if (normal.mag2() == 0.0) {
    normal = G4ThreeVector(0, 0, 1);
  } else {
    normal = normal.unit();
  }

  const G4double offset = offset_mm * mm;
  G4ThreeVector gunPos = rec.position - normal * offset;

  fGun->SetParticleDefinition(G4OpticalPhoton::OpticalPhotonDefinition());
  fGun->SetNumberOfParticles(1);
  fGun->SetParticlePosition(gunPos);
  fGun->SetParticleMomentumDirection(normal);
  fGun->SetParticleEnergy(energy_eV * eV);

  if (fMode != "gun") {
    SetGeneratorMode("gun");
  }
  fAnnounced = false;

  std::ios::fmtflags oldFlags = G4cout.flags();
  std::streamsize oldPrec = G4cout.precision();
  const double phi = std::atan2(rec.position.y(), rec.position.x()) * 180.0 / CLHEP::pi;
  double phi_norm = phi < 0.0 ? phi + 360.0 : phi;

  G4cout.setf(std::ios::fixed);
  G4cout << std::setprecision(2);
  G4cout << "[PHOTON_GUN] pmt=" << id
         << " pos=(" << gunPos.x()/mm << "," << gunPos.y()/mm << "," << gunPos.z()/mm << ") mm"
         << " dir=(" << normal.x() << "," << normal.y() << "," << normal.z() << ")"
         << " E=" << energy_eV << " eV"
         << " offset=" << offset_mm << " mm"
         << " phi_deg=" << phi_norm
         << G4endl;
  G4cout.flags(oldFlags);
  G4cout.precision(oldPrec);
}
