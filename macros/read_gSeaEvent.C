// ========================
// read_gSeaEvent.C
// ========================

// Update these if needed
#pragma cling add_include_path("/Users/jingyuanzhang/Desktop/CodeRepository/gSeaGen/src")
#pragma cling add_include_path("/Users/jingyuanzhang/Desktop/CodeRepository/gSeaGen/src/SeaEvent")
#pragma cling add_include_path("/Users/jingyuanzhang/Desktop/CodeRepository/GENIE-install/include")

#pragma cling add_include_path("/Users/jingyuanzhang/Desktop/CodeRepository/gSeaGen/src/SeaTrack")
#pragma cling add_include_path("/Users/jingyuanzhang/Desktop/CodeRepository/gSeaGen/src/GHEPWrapper")


gSystem->Load("libCore");
gSystem->Load("libPhysics");
gSystem->Load("libGeom");
gSystem->Load("libTree");

gSystem->Load("/Users/jingyuanzhang/Desktop/CodeRepository/GENIE-install/lib/libGFwMsg.dylib");
gSystem->Load("/Users/jingyuanzhang/Desktop/CodeRepository/GENIE-install/lib/libGFwReg.dylib");
gSystem->Load("/Users/jingyuanzhang/Desktop/CodeRepository/GENIE-install/lib/libGFwAlg.dylib");
gSystem->Load("/Users/jingyuanzhang/Desktop/CodeRepository/GENIE-install/lib/libGFwGHEP.dylib");
gSystem->Load("/Users/jingyuanzhang/Desktop/CodeRepository/GENIE-install/lib/libGFwEG.dylib");
gSystem->Load("/Users/jingyuanzhang/Desktop/CodeRepository/GENIE-install/lib/libGTlGeo.dylib");

gSystem->Load("/Users/jingyuanzhang/Desktop/CodeRepository/gSeaGen/lib/libGSeaEvent.dylib");

#include "GSeaEvent.h"


void read_gSeaEvent(const char* filename = "nu_e_CC_100GeV.100000000.et.root") {
    TFile* file = TFile::Open(filename);
    if (!file || file->IsZombie()) {
        std::cerr << "Error: Failed to open file " << filename << "\n";
        return;
    }

    TTree* tree = (TTree*)file->Get("Events");
    if (!tree) {
        std::cerr << "Error: TTree 'Events' not found in file\n";
        return;
    }

    GSeaEvent* evt = nullptr;
    tree->SetBranchAddress("Events", &evt);

    Long64_t nentries = tree->GetEntries();
    std::cout << "Found " << nentries << " event(s).\n";

    for (Long64_t i = 0; i < nentries; ++i) {
        tree->GetEntry(i);

        // Replace this with the actual member access once you see what fields exist
        std::cout << "Event " << i << ": "
                  << " Vertex = ("
                  << evt->Translate("InitX") << ", "
                  << evt->Translate("InitY") << ", "
                  << evt->Translate("InitZ") << "), ";

        std::cout << " Energy = "
                  << evt->Translate("InitE") << " GeV\n";
    }

    file->Close();
}