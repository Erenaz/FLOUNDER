// include/IO.hh
#pragma once
#include <TFile.h>
#include <TTree.h>
#include <string>
#include "G4UserRunAction.hh"
#include "Digitizer.hh"

class IORunAction : public G4UserRunAction {
public:
  IORunAction(const std::string& path, const DigitizerParams& P);
  ~IORunAction() override = default;

  void BeginOfRunAction(const G4Run*) override;
  void EndOfRunAction(const G4Run*) override;

  // exposed so EventAction can fill
  TFile* f = nullptr;
  TTree* thits = nullptr;
  TTree* tevents = nullptr;
  Digitizer dig;
  // branch caches (public so event action can write directly)
  int   b_event{}; short b_pmt{}; float b_t_ns{}; float b_npe{};
  int   e_event{}; int e_nprod{}; int e_nwall{}; int e_npmt{};
  float e_t0_ns{}; float e_tfirst_ns{}; float e_dfirst_mm{};
  float e_tof_ns{}; float e_res_ns{};
private:
  std::string outpath_;
};
