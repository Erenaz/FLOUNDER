#include "ActionInitialization.hh"
#include "PrimaryGeneratorAction.hh"
#include "RunAction.hh"
#include <G4VUserPrimaryGeneratorAction.hh>
#include "PhotonCountActions.hh"
#include "PhotonBudget.hh"
#include "IO.hh"
#include "PMTDigitizer.hh"
#include "G4OpticalParameters.hh"   // for FAST_MODE toggle (optional)

#include <cstdlib>
#include <utility>

ActionInitialization::ActionInitialization(const G4String& rfile,
                                           double zshift,
                                           RunProfileConfig profile)
: fRootFile(rfile), fZshift(zshift), fProfile(std::move(profile)) {}

 void ActionInitialization::Build() const {
  // Primary generator (supports rootracker or particle gun)
  SetUserAction(new PrimaryGeneratorAction(fRootFile, fZshift));

  // Run-level accounting
  SetUserAction(new RunAction());

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

  if (fProfile.enableDigitizer) {
    std::string digiCfg = fProfile.pmtConfigPath.empty()
                            ? "detector/config/pmt.yaml"
                            : fProfile.pmtConfigPath;
    std::string digiOut = fProfile.pmtOutputPath.empty()
                            ? "docs/day4/pmt_digi.root"
                            : fProfile.pmtOutputPath;
    SetUserAction(new PMTDigitizer(digiCfg, digiOut));
  }
}
