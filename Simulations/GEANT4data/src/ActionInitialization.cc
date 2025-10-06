#include "ActionInitialization.hh"
#include "RootrackerPrimaryGenerator.hh"
#include <G4VUserPrimaryGeneratorAction.hh>
#include "PhotonCountActions.hh"
#include "PhotonBudget.hh"
#include "IO.hh"
#include "Digitizer.hh"


ActionInitialization::ActionInitialization(const G4String& rfile, double zshift)
: fRootFile(rfile), fZshift(zshift) {}

void ActionInitialization::Build() const {
  auto* gen = new RootrackerPrimaryGenerator(fRootFile, fZshift);
  SetUserAction(gen);

  auto* evt = new PhotonCountEventAction();
  SetUserAction(evt);
  SetUserAction(new PhotonCountStackingAction(evt));

  // Day-3 budget counters + CSV
  auto* budgetEvt = new PhotonBudgetEventAction();
  budgetEvt->SetCSVPath("docs/day3/event_budget.csv"); // ensure docs/day3 exists
  SetUserAction(budgetEvt);
  SetUserAction(new PhotonBudgetSteppingAction(budgetEvt, "PMT"));

  // ---- Day-4 IO + Digitizer -----------------------------------------
  DigitizerParams P;
  // knobs via env; sensible defaults
  if (const char* s=getenv("DIGI_QE")) P.qe_flat = atof(s);
  if (const char* s=getenv("DIGI_TTS_NS")) P.tts_sigma_ns = atof(s);
  if (const char* s=getenv("DIGI_JIT_NS")) P.elec_jitter_ns = atof(s);
  if (const char* s=getenv("DIGI_DARK_HZ")) P.dark_rate_hz = atof(s);
  if (const char* s=getenv("DIGI_WIN_NS")) P.window_ns = atof(s);
  if (const char* s=getenv("DIGI_THR_PE")) P.thr_pe = atof(s);
  if (const char* s=getenv("DIGI_NCH")) P.n_pmt = std::max(1, atoi(s));

  auto* io = new IORunAction("docs/day4/hits.root", P);
  SetUserAction(io);

  // stash a pointer somewhere accessible to EventAction (see below)
  PhotonBudgetEventAction::SetIORun(io); // add a static setter on your event action
}

