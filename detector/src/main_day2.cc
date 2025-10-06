#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"
#include <G4OpticalPhysics.hh>
#include <G4OpticalParameters.hh>
#include <FTFP_BERT.hh>
#include <G4SystemOfUnits.hh>
#include <G4RunManagerFactory.hh>
#include <G4UImanager.hh>
#include <G4UIExecutive.hh>
#include <G4VisExecutive.hh>
#include <cstdlib>

int main(int argc, char** argv) {
  auto* runManager = G4RunManagerFactory::CreateRunManager(G4RunManagerType::Default);

  const char* gdml = std::getenv("G4_GDML");
  const char* rtrk = std::getenv("G4_ROOTRACKER");
  const char* zsh  = std::getenv("G4_ZSHIFT_MM");
  if(!gdml || !rtrk){
    G4Exception("main","Env",FatalException,"Set G4_GDML and G4_ROOTRACKER env vars.");
  }
  const double zshift = zsh ? atof(zsh) : 0.0;

  runManager->SetUserInitialization(new DetectorConstruction(gdml));
  {
    auto* phys = new FTFP_BERT;
    auto* opt  = new G4OpticalPhysics();
    auto* params = G4OpticalParameters::Instance();
    params->SetProcessActivation("Cerenkov", true);
    params->SetCerenkovMaxPhotonsPerStep(300);
    params->SetCerenkovTrackSecondariesFirst(true);
    phys->RegisterPhysics(opt);
    phys->SetDefaultCutValue(0.1*mm); // mm-scale cuts (optical not controlled by cuts)
    runManager->SetUserInitialization(phys);
  }
  runManager->SetUserInitialization(new ActionInitialization(rtrk, zshift));

  // Vis + UI
  auto* visManager = new G4VisExecutive; visManager->Initialize();
  auto* UImanager = G4UImanager::GetUIpointer();

  if (argc>1) {
    // Batch: macro handles /run/initialize & vis
    G4String cmd = "/control/execute ";
    UImanager->ApplyCommand(cmd + argv[1]);
  } else {
    // Interactive (or headless with default driver)
    auto* ui = new G4UIExecutive(argc, argv);
    UImanager->ApplyCommand("/run/initialize");

    const char* def = std::getenv("G4VIS_DEFAULT_DRIVER");
    if (def && *def) {
      UImanager->ApplyCommand(G4String("/vis/open ") + def);
    } else {
      UImanager->ApplyCommand("/vis/open TSG_OFFSCREEN 1200x900");
    }
    UImanager->ApplyCommand("/vis/drawVolume");
    UImanager->ApplyCommand("/vis/scene/add/trajectories smooth");
    UImanager->ApplyCommand("/vis/viewer/set/style surface");

    ui->SessionStart();
    delete ui;
  }

  delete visManager;
  delete runManager;
  return 0;
}
