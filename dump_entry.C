void dump_entry(const char* file, Long64_t i=0){
  TFile f(file); auto t=(TTree*)f.Get("gRooTracker");
  Double_t EvtVtx[4]; Int_t StdHepN, StdHepPdg[10000]; Double_t StdHepP4[10000][4];
  t->SetBranchAddress("EvtVtx",EvtVtx);
  t->SetBranchAddress("StdHepN",&StdHepN);
  t->SetBranchAddress("StdHepPdg",StdHepPdg);
  t->SetBranchAddress("StdHepP4",StdHepP4);
  t->GetEntry(i);
  int m=-1; for(int j=0;j<StdHepN;++j){ if(abs(StdHepPdg[j])==13) { m=j; break; } }
  printf("Evt %lld: vtx m=(%.6f,%.6f,%.6f) t=%.9f s; mu p4 GeV=(%.6f,%.6f,%.6f,%.6f)\n",
         i, EvtVtx[0], EvtVtx[1], EvtVtx[2], EvtVtx[3],
         StdHepP4[m][0],StdHepP4[m][1],StdHepP4[m][2],StdHepP4[m][3]);
}