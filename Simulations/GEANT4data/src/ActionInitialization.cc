#include "ActionInitialization.hh"
#include "RootrackerPrimaryGenerator.hh"
#include <G4VUserPrimaryGeneratorAction.hh>
#include "PhotonCountActions.hh"
#include "PhotonBudget.hh"
#include "IO.hh"
#include "Digitizer.hh"
#include "G4OpticalParameters.hh"   // for FAST_MODE toggle (optional)

ActionInitialization::ActionInitialization(const G4String& rfile, double zshift)
: fRootFile(rfile), fZshift(zshift) {}

 void ActionInitialization::Build() const {
  // Primary generator
  auto* gen = new RootrackerPrimaryGenerator(fRootFile, fZshift);
  SetUserAction(gen);

  // --- Day-2: photon counting baseline
  auto* pcEvt = new PhotonCountEventAction();
  SetUserAction(pcEvt);
  SetUserAction(new PhotonCountStackingAction(pcEvt));

  if (std::getenv("FAST_MODE")) {
  auto* op = G4OpticalParameters::Instance();
  op->SetProcessActivation("OpRayleigh", false); // disable scattering
  op->SetCerenkovMaxPhotonsPerStep(50);        // fewer photons per step
  op->SetCerenkovTrackSecondariesFirst(false); // don’t stall primaries
  // Add: op->SetBoundaryInvokeSD(true) as needed
  G4cout << "[FAST_MODE] Rayleigh OFF, CerenkovMaxPhotonsPerStep=50\n";
  }

  // Day-3 budget counters + CSV
  auto* budgetEvt = new PhotonBudgetEventAction();
  budgetEvt->SetCSVPath("docs/day3/event_budget.csv"); // ensure docs/day3 exists
  SetUserAction(budgetEvt);
  SetUserAction(new PhotonBudgetSteppingAction(budgetEvt, "PMT"));

  // --- Day-4: digitizer (QE thinning → PEs; TTS + jitter; dark; threshold)
  auto* digiEvt = new DigitizerEventAction();
  digiEvt->ConfigureFromEnv();               // reads DIGI_* env vars if set
  digiEvt->SetOutPath("docs/day4/hits.root");
  digiEvt->SetPMTMatch("PMT");               // adjust if GDML uses a different substring
  SetUserAction(digiEvt);
  SetUserAction(new DigitizerSteppingAction(digiEvt, "PMT"));
}

