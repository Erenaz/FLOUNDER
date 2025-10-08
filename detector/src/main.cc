#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"

#include <FTFP_BERT.hh>
#include <G4OpticalPhysics.hh>
#include <G4OpticalParameters.hh>
#include <G4RunManagerFactory.hh>
#include <G4SystemOfUnits.hh>
#include <G4UIExecutive.hh>
#include <G4UImanager.hh>
#include <G4VisExecutive.hh>

#include <cstdlib>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
  auto* runManager = G4RunManagerFactory::CreateRunManager(G4RunManagerType::Default);

  const char* gdml = std::getenv("G4_GDML");
  const char* rtrk = std::getenv("G4_ROOTRACKER");
  const char* zsh  = std::getenv("G4_ZSHIFT_MM");
  if(!gdml || !rtrk){
    G4Exception("main","Env",FatalException,"Set G4_GDML and G4_ROOTRACKER env vars.");
  }
  const double zshift = zsh ? atof(zsh) : 0.0;

  std::string profile = "day1";
  if (const char* envProfile = std::getenv("FLNDR_PROFILE")) {
    if (*envProfile) profile = envProfile;
  }

  std::string macroArg;
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (std::strncmp(arg, "--profile=", 10) == 0) {
      profile = std::string(arg + 10);
    } else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
      G4cout << "Usage: " << argv[0] << " [--profile=<name>] [macro.mac]\n"
             << "Profiles: day1 (default), day2, day3, custom\n";
      return 0;
    } else if (arg[0] == '-' && arg[1] == '-') {
      G4cout << "[WARN] Unknown option '" << arg << "' ignored.\n";
    } else {
      macroArg = arg;
    }
  }

  runManager->SetUserInitialization(new DetectorConstruction(gdml));

  auto* physicsList = new FTFP_BERT;
  auto* optical = new G4OpticalPhysics();
  auto* opticalParams = G4OpticalParameters::Instance();
  opticalParams->SetCerenkovMaxPhotonsPerStep(300);
  opticalParams->SetCerenkovMaxBetaChange(10.0);
  opticalParams->SetCerenkovTrackSecondariesFirst(true);
  opticalParams->SetProcessActivation("Cerenkov", true);
  if (profile == "day2" || profile == "day3") {
    physicsList->SetDefaultCutValue(0.1 * mm);
    G4cout << "[CFG] Applied profile '" << profile << "': default cut = 0.1 mm\n";
  } else {
    G4cout << "[CFG] Applied profile '" << profile << "'\n";
  }
  physicsList->RegisterPhysics(optical);
  G4cout << "[OPT] optical physics registered" << G4endl;
  runManager->SetUserInitialization(physicsList);
  runManager->SetUserInitialization(new ActionInitialization(rtrk, zshift));

  // Vis + UI
  auto* visManager = new G4VisExecutive; visManager->Initialize();
  auto* UImanager = G4UImanager::GetUIpointer();

  if (!macroArg.empty()) {
    // Batch: macro handles /run/initialize & vis
    G4String cmd = "/control/execute ";
    UImanager->ApplyCommand(cmd + macroArg);
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
