// src/IO.cc
#include "IO.hh"
#include <TNamed.h>

IORunAction::IORunAction(const std::string& path, const DigitizerParams& P)
: G4UserRunAction(), outpath_(path), dig(P) {}

void IORunAction::BeginOfRunAction(const G4Run*) {
  f = TFile::Open(outpath_.c_str(), "RECREATE");
  thits = new TTree("hits","digitized hits");
  thits->Branch("event",&b_event,"event/I");
  thits->Branch("pmt",&b_pmt,"pmt/S");
  thits->Branch("t_ns",&b_t_ns,"t_ns/F");
  thits->Branch("npe",&b_npe,"npe/F");

  tevents = new TTree("events","event summary");
  tevents->Branch("event",&e_event,"event/I");
  tevents->Branch("n_produced",&e_nprod,"n_produced/I");
  tevents->Branch("n_wall",&e_nwall,"n_wall/I");
  tevents->Branch("n_pmt",&e_npmt,"n_pmt/I");
  tevents->Branch("t0_ns",&e_t0_ns,"t0_ns/F");
  tevents->Branch("t_first_ns",&e_tfirst_ns,"t_first_ns/F");
  tevents->Branch("d_first_mm",&e_dfirst_mm,"d_first_mm/F");
  tevents->Branch("tof_geom_ns",&e_tof_ns,"tof_geom_ns/F");
  tevents->Branch("first_residual_ns",&e_res_ns,"first_residual_ns/F");

  // minimal metadata; you can extend this to include geometry/optics hashes
  new TNamed("geometry_hash","<fill me in>");
  new TNamed("optics_config","<fill me in>");
}

void IORunAction::EndOfRunAction(const G4Run*) {
  f->Write();
  f->Close();
  delete f; f=nullptr; thits=nullptr; tevents=nullptr;
}
