#include "PhotonBudget.hh"
#include "Digitizer.hh"
#include "IO.hh" 
#include "RunManifest.hh"
#include "G4Event.hh"
#include "G4OpticalPhoton.hh"
#include "G4VPhysicalVolume.hh"
#include "G4Track.hh"
#include "G4LogicalVolume.hh"
#include "G4RunManager.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"
#include "G4TouchableHandle.hh"

#include <unordered_set>
#include <fstream>
#include <iomanip>
#include <regex>
#include <limits>

static G4ThreeVector g_x0(0,0,0);
static double g_t0_ns = 0.0;
static IORunAction* gIO = nullptr;      // NEW: I/O owner (set from ActionInitialization)

void PrimaryInfo::Set(const G4ThreeVector& x0, double t0_ns){ g_x0=x0; g_t0_ns=t0_ns; }
const G4ThreeVector& PrimaryInfo::X0(){ return g_x0; }
double PrimaryInfo::T0ns(){ return g_t0_ns; }

std::string PhotonBudgetEventAction::s_csv_path = "docs/day4/event_budget.csv"; // NEW default
bool PhotonBudgetEventAction::s_csv_header_written = false;
void PhotonBudgetEventAction::SetIORun(IORunAction* io){ gIO = io; }            // NEW

void PhotonBudgetEventAction::BeginOfEventAction(const G4Event*) {
  nProduced = nAtWall = nAtPMT = 0;
  firstResidualNs = std::numeric_limits<double>::quiet_NaN();
  // --- Day-4 enriched timing/geometry
  t0_ns       = PrimaryInfo::T0ns();
  t_first_ns  = std::numeric_limits<double>::quiet_NaN();
  d_first_mm  = std::numeric_limits<double>::quiet_NaN();
  tof_geom_ns = std::numeric_limits<double>::quiet_NaN();
  first_kind.clear();
  candidates.clear();
}

void PhotonBudgetEventAction::EndOfEventAction(const G4Event* ev) {
  // --- (A) CSV: lazy-create & append
  std::ofstream out(s_csv_path, std::ios::app);
  if (!s_csv_header_written) {
    out << "event,n_produced,n_wall,n_pmt"
        << ",t0_ns,t_first_ns,d_first_mm,tof_geom_ns,first_residual_ns,first_kind\n";
    s_csv_header_written = true;
  }
  out << ev->GetEventID() << ","
      << nProduced << ","
      << nAtWall   << ","
      << nAtPMT    << ","
      << (std::isfinite(t0_ns)       ? t0_ns       : 0.0) << ","
      << (std::isfinite(t_first_ns)  ? t_first_ns  : 0.0) << ","
      << (std::isfinite(d_first_mm)  ? d_first_mm  : 0.0) << ","
      << (std::isfinite(tof_geom_ns) ? tof_geom_ns : 0.0) << ","
      << (std::isfinite(firstResidualNs) ? firstResidualNs : 0.0) << ","
      << (first_kind.empty() ? "NA" : first_kind)
      << "\n";
  // also print a compact line for the log
  const auto& cfg = GetRunManifest();
  if (!cfg.quiet && cfg.opticalVerboseLevel > 0) {
    G4cout << "[Budget] evt=" << ev->GetEventID()
           << " Nprod=" << nProduced
           << " Nwall=" << nAtWall
           << " Npmt="  << nAtPMT
           << " firstΔt(ns)=" << (std::isfinite(firstResidualNs)? firstResidualNs : -1.0)
           << G4endl;
  }

    // --- (B) Digitize & write ROOT (optional; only if run action provided)
  if (gIO) {
    std::vector<DigiHit> dh;
    gIO->dig.Digitize(ev->GetEventID(), candidates, dh);
    gIO->dig.AddDarkNoise(ev->GetEventID(), t0_ns, dh);
    // hits tree
    for (const auto& h : dh) {
      gIO->b_event = h.event;
      gIO->b_pmt   = (short)h.pmt;
      gIO->b_t_ns  = h.t_ns;
      gIO->b_npe   = h.npe;
      gIO->thits->Fill();
    }
    // events tree (carry Day-3 summary + enriched timing)
    gIO->e_event    = ev->GetEventID();
    gIO->e_nprod    = nProduced;
    gIO->e_nwall    = nAtWall;
    gIO->e_npmt     = nAtPMT;
    gIO->e_t0_ns    = t0_ns;
    gIO->e_tfirst_ns= t_first_ns;
    gIO->e_dfirst_mm= d_first_mm;
    gIO->e_tof_ns   = tof_geom_ns;
    gIO->e_res_ns   = firstResidualNs;
    gIO->tevents->Fill();
  }
}

void PhotonBudgetEventAction::SetCSVPath(const std::string& path) { s_csv_path = path; }

PhotonBudgetSteppingAction::PhotonBudgetSteppingAction(PhotonBudgetEventAction* evt,
                                                       std::string patt)
: evt_(evt), patt_(std::move(patt)) {}

void PhotonBudgetSteppingAction::UserSteppingAction(const G4Step* step) {

  // --- per-event reset and per-track de-dup (thread-local, auto-cleared on event change)
  static thread_local int s_last_eid = -1;
  static thread_local std::unordered_set<int> seen_wall, seen_pmt;
  auto* rm = G4RunManager::GetRunManager();
  int eid = rm->GetCurrentEvent()->GetEventID();
  if (eid != s_last_eid) {
    s_last_eid = eid;
    seen_wall.clear();
    seen_pmt.clear();
    firstRecorded_ = false;
  }

  auto* trk = step->GetTrack();
  if (trk->GetDefinition() != G4OpticalPhoton::Definition()) return;

  // Count produced photons at their first step
  if (trk->GetCurrentStepNumber() == 1) {
    ++(evt_->nProduced);
  }

  // Boundary / volume transitions
  auto* prePV  = step->GetPreStepPoint()->GetPhysicalVolume();
  auto* postPV = step->GetPostStepPoint()->GetPhysicalVolume();
  if (!prePV || !postPV) return;

  // helper: fill the "first" timing block once
  auto record_first = [&](const char* kind){
    const double n_eff = 1.33;
    const double t_ns  = trk->GetGlobalTime()/ns;
    const auto   x     = step->GetPostStepPoint()->GetPosition();
    const auto   dx    = (x - PrimaryInfo::X0());
    const double dmm   = dx.mag()/mm;
    const double tof   = dx.mag() / (CLHEP::c_light/n_eff) / ns;
    evt_->t_first_ns   = t_ns;
    evt_->d_first_mm   = dmm;
    evt_->tof_geom_ns  = tof;
    evt_->firstResidualNs = t_ns - evt_->t0_ns - tof;
    evt_->first_kind   = kind;
    firstRecorded_ = true;
  };

  // wall hit: leaving water (can) into world
  if (prePV != postPV && postPV->GetMotherLogical() == nullptr) {
    // count each photon at the wall at most once
    if (seen_wall.insert(trk->GetTrackID()).second) {
      ++(evt_->nAtWall);
    }
    // record first time residual if not yet set (use this if no PMT later)
    if (!firstRecorded_) {
      record_first("WALL");
    }
  }

  // PMT hit: postPV name contains "PMT"
  if (prePV != postPV) {
    const auto& name = postPV->GetName();
    if (name.find(patt_) != std::string::npos) {
      // count each photon at the PMT at most once
      if (seen_pmt.insert(trk->GetTrackID()).second) {
        ++(evt_->nAtPMT);
        // collect candidate for digitizer
        const auto& touch = step->GetPostStepPoint()->GetTouchableHandle();
        const int pmt_id  = touch->GetCopyNumber(); // adjust depth if needed
        const double t_ns = trk->GetGlobalTime()/ns;
        const double e_eV = trk->GetTotalEnergy()/eV;
        const double lambda_nm = (e_eV>0 ? 1239.84193/e_eV : 0.0);
        evt_->candidates.push_back(HitCandidate{pmt_id, t_ns, lambda_nm});
      }
      if (!firstRecorded_) {
        record_first("PMT");
      }
    }
  }
}
