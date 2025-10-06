#pragma once
#include "G4UserEventAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4ThreeVector.hh"
#include "Digitizer.hh"

#include <string>
#include <vector>     // NEW: we keep PMT candidates for digitizer
#include <limits>     // NEW: we use std::numeric_limits in the header

// forward decls to avoid heavy includes in the header
class IORunAction;    // owner of ROOT file + Digitizer (set once from ActionInitialization)

struct PrimaryInfo {
  static void Set(const G4ThreeVector& x0, double t0_ns);
  static const G4ThreeVector& X0();
  static double T0ns();
};

class PhotonBudgetEventAction : public G4UserEventAction {
public:
  PhotonBudgetEventAction() = default;
  void BeginOfEventAction(const G4Event*) override;
  void EndOfEventAction(const G4Event*) override;
  // counters this event
  unsigned long long nProduced = 0;
  unsigned long long nAtWall   = 0;
  unsigned long long nAtPMT    = 0;
  double firstResidualNs = std::numeric_limits<double>::quiet_NaN();
  // --- Day-4 enriched timing/geometry (for QC)
  double t0_ns       = 0.0;                                      // event start time we set from PrimaryInfo
  double t_first_ns  = std::numeric_limits<double>::quiet_NaN(); // time of first qualifying hit (PMT if present, else WALL)
  double d_first_mm  = std::numeric_limits<double>::quiet_NaN(); // |x_first - x0| in mm
  double tof_geom_ns = std::numeric_limits<double>::quiet_NaN(); // n·d/c in ns (geometric)
  std::string first_kind;                                        // "PMT" or "WALL"
  // --- PMT candidates collected in stepping; digitizer consumes them
  std::vector<HitCandidate> candidates;

  // config
  static void SetCSVPath(const std::string& path);
  static void SetIORun(IORunAction* io); // NEW: optional hook so EndOfEvent can write ROOT hits/events

private:
  static std::string s_csv_path;
  static bool s_csv_header_written;
};

class PhotonBudgetSteppingAction : public G4UserSteppingAction {
public:
  explicit PhotonBudgetSteppingAction(PhotonBudgetEventAction* evt,
                                      std::string pmtNamePattern="PMT");
  void UserSteppingAction(const G4Step* step) override;
private:
  PhotonBudgetEventAction* evt_;
  std::string patt_;
  bool firstRecorded_ = false;
};
