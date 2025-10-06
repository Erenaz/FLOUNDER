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
private:
  std::string outpath_;
  // branches
  int b_event; short b_pmt; float b_t_ns; float b_npe;
  int e_event, e_nprod, e_nwall, e_npmt;
  float e_t0_ns, e_tfirst_ns, e_dfirst_mm, e_tof_ns, e_res_ns;
};

