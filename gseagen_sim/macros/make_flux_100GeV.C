void make_flux_100GeV() {
  TFile *f = new TFile("../flux/nu_e_flux_100GeV.root", "RECREATE");

  // A simple histogram for νₑ with a single energy bin at 100 GeV
  TH1D* hFlux = new TH1D("nu_e_flux", "Fake nu_e flux", 1, 99.5, 100.5);
  hFlux->SetBinContent(1, 1.0); // 1 particle in this bin

  hFlux->GetXaxis()->SetTitle("Neutrino Energy (GeV)");
  hFlux->GetYaxis()->SetTitle("Flux (arbitrary units)");

  hFlux->Write();
  f->Close();
}

