#include "Digitizer.hh"
#include "PhotonBudget.hh"          // for PrimaryInfo::T0ns() and X0()
#include "RunManifest.hh"
#include "G4RunManager.hh"
#include "G4Step.hh"
#include "G4HCofThisEvent.hh"
#include "G4VHitsCollection.hh"
#include "G4SystemOfUnits.hh"
#include "G4OpticalPhoton.hh"
#include "G4OpBoundaryProcess.hh"
#include "G4PhysicalConstants.hh"
#include "G4ProcessManager.hh"
#include "G4Poisson.hh"
#include "G4Track.hh"
#include "G4VProcess.hh"
#include "Randomize.hh"

#include "TFile.h"
#include "TTree.h"
#include "TNamed.h"

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <unordered_set>

namespace {

const char* BoundaryStatusName(G4OpBoundaryProcessStatus status) {
  switch (status) {
    case Undefined: return "Undefined";
    case Transmission: return "Transmission";
    case FresnelRefraction: return "FresnelRefraction";
    case FresnelReflection: return "FresnelReflection";
    case TotalInternalReflection: return "TotalInternalReflection";
    case LambertianReflection: return "LambertianReflection";
    case LobeReflection: return "LobeReflection";
    case SpikeReflection: return "SpikeReflection";
    case BackScattering: return "BackScattering";
    case Absorption: return "Absorption";
    case Detection: return "Detection";
    case NotAtBoundary: return "NotAtBoundary";
    case SameMaterial: return "SameMaterial";
    case StepTooSmall: return "StepTooSmall";
    case NoRINDEX: return "NoRINDEX";
    case PolishedLumirrorAirReflection: return "PolishedLumirrorAirReflection";
    case PolishedLumirrorGlueReflection: return "PolishedLumirrorGlueReflection";
    case PolishedAirReflection: return "PolishedAirReflection";
    case PolishedTeflonAirReflection: return "PolishedTeflonAirReflection";
    case PolishedTiOAirReflection: return "PolishedTiOAirReflection";
    case PolishedTyvekAirReflection: return "PolishedTyvekAirReflection";
    case PolishedVM2000AirReflection: return "PolishedVM2000AirReflection";
    case PolishedVM2000GlueReflection: return "PolishedVM2000GlueReflection";
    case EtchedLumirrorAirReflection: return "EtchedLumirrorAirReflection";
    case EtchedLumirrorGlueReflection: return "EtchedLumirrorGlueReflection";
    case EtchedAirReflection: return "EtchedAirReflection";
    case EtchedTeflonAirReflection: return "EtchedTeflonAirReflection";
    case EtchedTiOAirReflection: return "EtchedTiOAirReflection";
    case EtchedTyvekAirReflection: return "EtchedTyvekAirReflection";
    case EtchedVM2000AirReflection: return "EtchedVM2000AirReflection";
    case EtchedVM2000GlueReflection: return "EtchedVM2000GlueReflection";
    case GroundLumirrorAirReflection: return "GroundLumirrorAirReflection";
    case GroundLumirrorGlueReflection: return "GroundLumirrorGlueReflection";
    case GroundAirReflection: return "GroundAirReflection";
    case GroundTeflonAirReflection: return "GroundTeflonAirReflection";
    case GroundTiOAirReflection: return "GroundTiOAirReflection";
    case GroundTyvekAirReflection: return "GroundTyvekAirReflection";
    case GroundVM2000AirReflection: return "GroundVM2000AirReflection";
    case GroundVM2000GlueReflection: return "GroundVM2000GlueReflection";
    case Dichroic: return "Dichroic";
    default: return "Unknown";
  }
}

G4OpBoundaryProcess* FindBoundaryProcess(const G4Track* track) {
  static thread_local G4OpBoundaryProcess* cached = nullptr;
  if (cached) return cached;
  if (!track) return nullptr;
  auto* procMgr = track->GetDefinition()->GetProcessManager();
  if (!procMgr) return nullptr;
  auto* procList = procMgr->GetProcessList();
  for (G4int i = 0; i < procMgr->GetProcessListLength(); ++i) {
    auto* proc = (*procList)[i];
    if (auto* boundary = dynamic_cast<G4OpBoundaryProcess*>(proc)) {
      cached = boundary;
      break;
    }
  }
  return cached;
}

} // namespace

// ---------------- HitWriter (ROOT) ----------------
struct HitWriter::Impl {
  TFile* f{};
  TTree* t{};
  // branches
  int    b_event{};
  int    b_pmt{};
  double b_t_ns{};
  double b_npe{};
};
HitWriter::HitWriter(const std::string& outroot) : p_(new Impl) {
  std::filesystem::create_directories(std::filesystem::path(outroot).parent_path());
  p_->f = TFile::Open(outroot.c_str(),"RECREATE");
  p_->t = new TTree("hits","Digitized PMT hits");
  p_->t->Branch("event",&p_->b_event);
  p_->t->Branch("pmt",&p_->b_pmt);
  p_->t->Branch("t_ns",&p_->b_t_ns);
  p_->t->Branch("npe",&p_->b_npe);
  p_->t->SetDirectory(p_->f);
  p_->f->SetCompressionSettings(209); // algo 2 (lz4), level 9: good size/speed
  RegisterOutputFile(p_->f);
}
HitWriter::~HitWriter(){
  if (p_ && p_->f) {
    p_->f->cd();
    p_->t->Write();
    p_->f->Write();
    p_->f->Close();
  }
}
void HitWriter::writeRunMeta(const std::string& geom_hash, const std::string& optics_note){
  if (!p_ || !p_->f) return;
  p_->f->cd();
  new TNamed("geometry_hash", geom_hash.c_str());
  new TNamed("optics_config", optics_note.c_str());
}
void HitWriter::writeEvent(const std::vector<DigiHit>& hits){
  for (const auto& h : hits) {
    p_->b_event = h.event;
    p_->b_pmt   = h.pmt;
    p_->b_t_ns  = h.t_ns;
    p_->b_npe   = h.npe;
    p_->t->Fill();
  }
}

// ---------------- Digitizer (standalone helper) ----------------
Digitizer::Digitizer(const DigitizerParams& params) : params_(params) {}

double Digitizer::gauss(double sigma_ns) const {
  return sigma_ns > 0.0 ? G4RandGauss::shoot(0.0, sigma_ns) : 0.0;
}

void Digitizer::Digitize(int event_id, const std::vector<HitCandidate>& candidates,
                         std::vector<DigiHit>& out_hits) const {
  last_event_pmts_.clear();
  std::unordered_set<int> seen;
  seen.reserve(candidates.size());

  const double t0_ns = PrimaryInfo::T0ns();

  for (const auto& cand : candidates) {
    if (seen.insert(cand.pmt).second) {
      last_event_pmts_.push_back(cand.pmt);
    }

    if (G4UniformRand() > params_.QE) continue;

    const double t_digi = cand.t_ns + gauss(params_.TTS_ns) + gauss(params_.JITTER_ns);
    const double dt     = t_digi - t0_ns;
    if (dt < params_.TWIN_LO_ns || dt > params_.TWIN_HI_ns) continue;
    if (1.0 < params_.THRESH_PE) continue; // simple single-PE model

    out_hits.push_back({event_id, cand.pmt, t_digi, 1.0});
  }
}

void Digitizer::AddDarkNoise(int event_id, double t0_ns, std::vector<DigiHit>& out_hits) const {
  if (params_.DARK_HZ <= 0.0) return;

  const double win_ns = params_.TWIN_HI_ns - params_.TWIN_LO_ns;
  if (win_ns <= 0.0) return;

  std::unordered_set<int> pmts(last_event_pmts_.begin(), last_event_pmts_.end());
  for (const auto& hit : out_hits) {
    pmts.insert(hit.pmt);
  }
  if (pmts.empty()) return;

  const double win_s = win_ns * 1e-9;
  const double mean_per_pmt = params_.DARK_HZ * win_s;

  for (int pmt : pmts) {
    const int k = G4Poisson(mean_per_pmt);
    for (int i = 0; i < k; ++i) {
      const double t = t0_ns + params_.TWIN_LO_ns + G4UniformRand() * win_ns;
      out_hits.push_back({event_id, pmt, t, 1.0});
    }
  }
}

// ---------------- Digitizer ----------------
DigitizerEventAction::DigitizerEventAction() { }

void DigitizerEventAction::ConfigureFromEnv(){
  auto dbl = [](const char* k, double v)->double {
    if (const char* s = std::getenv(k)) try { return std::stod(s); } catch(...) {}
    return v;
  };
  auto str = [](const char* k, const char* v)->std::string {
    if (const char* s = std::getenv(k)) return std::string(s);
    return std::string(v);
  };
  out_path_        = str("DIGI_OUT", out_path_.c_str());
  patt_            = str("BUDGET_PMT_MATCH", patt_.c_str()); // reuse same var
  params_.QE       = dbl("DIGI_QE", params_.QE);
  params_.TTS_ns   = dbl("DIGI_TTS_NS", params_.TTS_ns);
  params_.JITTER_ns= dbl("DIGI_JITTER_NS", params_.JITTER_ns);
  params_.DARK_HZ  = dbl("DIGI_DARK_HZ", params_.DARK_HZ);
  params_.THRESH_PE= dbl("DIGI_THRESHOLD_PE", params_.THRESH_PE);
  params_.TWIN_LO_ns = dbl("DIGI_TWIN_LO_NS", params_.TWIN_LO_ns);
  params_.TWIN_HI_ns = dbl("DIGI_TWIN_HI_NS", params_.TWIN_HI_ns);
}

void DigitizerEventAction::BeginOfEventAction(const G4Event*){
  const auto* rm = G4RunManager::GetRunManager();
  evtid_  = rm->GetCurrentEvent()->GetEventID();
  t0_ns_  = PrimaryInfo::T0ns();
  hits_ev_.clear();
  if (!writer_) {
    writer_ = std::make_unique<HitWriter>(out_path_);
    writer_->writeRunMeta("<set_with_sha1sum_gdml>", "wallModel=…; rho=…; water=…");
  }

  if (GetRunManifest().opticalDebug && evtid_ == 0) {
    G4cout << "[OPT_DBG] Event 0: optical boundary tracing enabled (limited output)" << G4endl;
  }
  cerenkovSecondaries_ = 0;
}

void DigitizerEventAction::EndOfEventAction(const G4Event* event){
  dumpHitCollections(event);
  // Add dark noise after photon processing
  addDarkNoise();
  G4cout << "[OPT_DBG] event=" << evtid_
         << " n_cerenkov_secondaries=" << cerenkovSecondaries_ << G4endl;
  // write and clear
  writer_->writeEvent(hits_ev_);
  hits_ev_.clear();
}

int DigitizerEventAction::IdForPMT(const G4VPhysicalVolume* pv){
  auto it = pmt_id_.find(pv);
  if (it != pmt_id_.end()) return it->second;
  int id = next_id_++;
  pmt_id_.emplace(pv,id);
  return id;
}

double DigitizerEventAction::gauss(double sigma_ns){
  return sigma_ns > 0 ? G4RandGauss::shoot(0.0, sigma_ns) : 0.0;
}

void DigitizerEventAction::PushPhotonAtPMT(const G4ThreeVector& x, double t_ns){
  (void)x; // position not yet used in this simplified digitizer
  // QE thinning
  if (G4UniformRand() > params_.QE) return;

  // timing smear
  double t_digi = t_ns + gauss(params_.TTS_ns) + gauss(params_.JITTER_ns);

  // time window relative to event t0
  double dt = t_digi - t0_ns_;
  if (dt < params_.TWIN_LO_ns || dt > params_.TWIN_HI_ns) return;

  // threshold: here we model single PE; use npe=1 if above threshold
  if (1.0 < params_.THRESH_PE) return;

  // PMT identity from geometry (post-step PV)
  // NOTE: stepping action passes us that PV, so we set it there
  // We can’t get PV here, so stepping will call IdForPMT and push
}

void DigitizerEventAction::addDarkNoise(){
  if (params_.DARK_HZ <= 0 || pmt_id_.empty()) return;
  // homogeneous Poisson per PMT in window
  double win_s = std::max(0.0, params_.TWIN_HI_ns - params_.TWIN_LO_ns) * 1e-9;
  double mean_per_pmt = params_.DARK_HZ * win_s;
  for (const auto& kv : pmt_id_) {
    int id = kv.second;
    int k = G4Poisson(mean_per_pmt);
    for (int i=0;i<k;++i) {
      double t = t0_ns_ + params_.TWIN_LO_ns + G4UniformRand()*(params_.TWIN_HI_ns - params_.TWIN_LO_ns);
      hits_ev_.push_back({evtid_, id, t, 1.0});
    }
  }
}

void DigitizerEventAction::dumpHitCollections(const G4Event* event) const {
  if (!event) {
    G4cout << "[HCE] (no event)" << G4endl;
    return;
  }
  auto* hce = event->GetHCofThisEvent();
  if (!hce) {
    G4cout << "[HCE] (null)" << G4endl;
    return;
  }
  const G4int n = hce->GetNumberOfCollections();
  for (G4int i = 0; i < n; ++i) {
    auto* hc = hce->GetHC(i);
    const G4String name = hc ? hc->GetName() : "(null)";
    const G4int size = hc ? hc->GetSize() : 0;
    G4cout << "[HCE] idx=" << i << " name=" << name << " size=" << size << G4endl;
  }
}

// ---------------- Stepping: hook PMT crossings ----------------
void DigitizerSteppingAction::UserSteppingAction(const G4Step* step){
  auto* trk = step->GetTrack();
  if (trk->GetDefinition() != G4OpticalPhoton::Definition()) return;

  if (trk->GetCurrentStepNumber() == 1) {
    auto* creator = trk->GetCreatorProcess();
    if (creator && creator->GetProcessName() == "Cerenkov") {
      evt_->IncrementCerenkovSecondary();
    }
  }

  auto* prePV  = step->GetPreStepPoint()->GetPhysicalVolume();
  auto* postPV = step->GetPostStepPoint()->GetPhysicalVolume();
  if (!prePV || !postPV) return;
  if (prePV == postPV) return;

  const auto& name = postPV->GetName();
  if (name.find(patt_) == std::string::npos) return;

  if (GetRunManifest().opticalDebug) {
    static const int kMaxReports = 100;
    static int reported = 0;
    if (reported < kMaxReports) {
      auto* boundary = FindBoundaryProcess(trk);
      const char* statusName = boundary ? BoundaryStatusName(boundary->GetStatus()) : "n/a";
      G4cout << "[OPT_DBG] evt=" << G4RunManager::GetRunManager()->GetCurrentEvent()->GetEventID()
             << " pre=" << prePV->GetName()
             << " post=" << postPV->GetName()
             << " status=" << statusName
             << " t_ns=" << trk->GetGlobalTime() / ns << G4endl;
      if (++reported == kMaxReports) {
        G4cout << "[OPT_DBG] ... further boundary logs suppressed ..." << G4endl;
      }
    }
  }

  // time & position at PMT boundary
  double t_ns = trk->GetGlobalTime()/ns;
  // Assign PMT id, create hit
  int pid = evt_->IdForPMT(postPV);
  // DigitizerEventAction::PushPhotonAtPMT needs the PV for id,
  // so push directly here with the id resolved:
  // model: single-PE per survived photon (QE thinning already inside)
  if (G4UniformRand() <=  evt_->params_.QE) { // reuse QE here for speed
    double t_digi = t_ns + evt_->gauss(evt_->params_.TTS_ns) + evt_->gauss(evt_->params_.JITTER_ns);
    double dt = t_digi - evt_->t0_ns_;
    if (dt >= evt_->params_.TWIN_LO_ns && dt <= evt_->params_.TWIN_HI_ns && 1.0 >= evt_->params_.THRESH_PE) {
      evt_->hits_ev_.push_back({G4RunManager::GetRunManager()->GetCurrentEvent()->GetEventID(),
                                pid, t_digi, 1.0});
    }
  }
}

void DigitizerEventAction::IncrementCerenkovSecondary() {
  ++cerenkovSecondaries_;
}
