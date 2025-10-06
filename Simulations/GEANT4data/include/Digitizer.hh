#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include "G4UserEventAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4VPhysicalVolume.hh"
#include "G4ThreeVector.hh"

struct HitCandidate {
  int    pmt{};
  double t_ns{};
  double lambda_nm{};
};

struct DigitizerParams {
  double QE            = 0.25;    // flat QE (0..1)
  double TTS_ns        = 1.3;     // transit-time spread
  double JITTER_ns     = 0.5;     // electronics jitter
  double DARK_HZ       = 3000.0;  // per-PMT dark rate
  double THRESH_PE     = 0.30;    // discriminator threshold (PE)
  double TWIN_LO_ns    = -50.0;   // time window lower (from t0)
  double TWIN_HI_ns    = 1200.0;  // time window upper (from t0)
};

struct DigiHit {
  int    event{};
  int    pmt{};
  double t_ns{};      // digitized time (with TTS + jitter)
  double npe{};       // charge in PEs (>= threshold)
};

class Digitizer {
public:
  explicit Digitizer(const DigitizerParams& params = DigitizerParams());

  const DigitizerParams& Params() const { return params_; }

  void Digitize(int event_id, const std::vector<HitCandidate>& candidates,
                std::vector<DigiHit>& out_hits) const;
  void AddDarkNoise(int event_id, double t0_ns, std::vector<DigiHit>& out_hits) const;

private:
  double gauss(double sigma_ns) const;
  DigitizerParams params_;
  mutable std::vector<int> last_event_pmts_;
};

class HitWriter {
public:
  explicit HitWriter(const std::string& outroot);
  ~HitWriter();
  void writeRunMeta(const std::string& geom_hash, const std::string& optics_note);
  void writeEvent(const std::vector<DigiHit>& hits);

private:
  struct Impl; std::unique_ptr<Impl> p_;
};

class DigitizerEventAction : public G4UserEventAction {
public:
  DigitizerEventAction();
  void BeginOfEventAction(const G4Event*) override;
  void EndOfEventAction(const G4Event*) override;

  // runtime config
  void SetOutPath(const std::string& p) { out_path_ = p; }
  void SetPMTMatch(const std::string& s) { patt_ = s; }

  // physics knobs (env-overridable)
  void ConfigureFromEnv();

  // interface used by the stepping action:
  int  IdForPMT(const G4VPhysicalVolume* pv);
  void PushPhotonAtPMT(const G4ThreeVector& x, double t_ns);

private:
  // config
  std::string out_path_ = "docs/day4/hits.root";
  std::string patt_     = "PMT";
  DigitizerParams params_{};

  // state
  int  evtid_{-1};
  double t0_ns_{0.0};
  std::unordered_map<const G4VPhysicalVolume*, int> pmt_id_;
  int next_id_{0};
  std::vector<DigiHit> hits_ev_;
  std::unique_ptr<HitWriter> writer_;

  // helpers
  void addDarkNoise();
  double gauss(double sigma_ns);
  friend class DigitizerSteppingAction;
};
  
class DigitizerSteppingAction : public G4UserSteppingAction {
public:
  explicit DigitizerSteppingAction(DigitizerEventAction* E, std::string patt)
    : evt_(E), patt_(std::move(patt)) {}
  void UserSteppingAction(const G4Step* step) override;
private:
  DigitizerEventAction* evt_{};
  std::string patt_;
};
