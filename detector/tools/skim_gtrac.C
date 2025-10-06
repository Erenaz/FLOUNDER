void skim_gtrac(const char* src, const char* dst, Long64_t N=2000) {
  TFile fin(src, "READ");
  auto* t = dynamic_cast<TTree*>(fin.Get("gRooTracker"));
  if (!t) { std::cerr << "No gRooTracker in " << src << "\n"; return; }
  Long64_t n = t->GetEntries();
  Long64_t M = std::min(N, n);

  gROOT->SetBatch(kTRUE);
  TFile fout(dst, "RECREATE");
  auto* t2 = t->CloneTree(0);
  for (Long64_t i = 0; i < M; ++i) {
    t->GetEntry(i);
    t2->Fill();
  }
  t2->Write();
  fout.Close();
  std::cout << "Wrote " << M << " entries to " << dst << "\n";
}
