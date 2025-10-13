#include "DetectorConstruction.hh"
#include "ActionInitialization.hh"
#include "PhysicsList.hh"
#include "RunManifest.hh"

#include <G4RunManagerFactory.hh>
#include <G4SystemOfUnits.hh>
#include <G4UIExecutive.hh>
#include <G4UImanager.hh>
#include <G4VisExecutive.hh>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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

  bool opticsExplicit = false;
  std::string opticsConfig = "detector/config/optics.yaml";
  if (const char* envOptics = std::getenv("FLNDR_OPTICS_CONFIG")) {
    if (*envOptics) {
      opticsConfig = envOptics;
      opticsExplicit = true;
    }
  }

  std::string macroArg;
  std::string optEnableOverride;
  bool pmtExplicit = false;
  std::string pmtConfig = "";
  bool optDebug = false;
  int checkOverlapsN = 0;
  double qeOverride = std::numeric_limits<double>::quiet_NaN();
  double qeFlat = std::numeric_limits<double>::quiet_NaN();
  bool quiet = false;
  int optVerbose = 0;
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (std::strncmp(arg, "--profile=", 10) == 0) {
      profile = std::string(arg + 10);
    } else if (std::strncmp(arg, "--optics=", 9) == 0) {
      opticsConfig = std::string(arg + 9);
      opticsExplicit = true;
    } else if (std::strcmp(arg, "--optics") == 0) {
      if (i + 1 < argc) {
        opticsConfig = std::string(argv[++i]);
        opticsExplicit = true;
      } else {
        G4cout << "[WARN] --optics flag expects a value; using existing value '"
               << opticsConfig << "'\n";
      }
    } else if (std::strncmp(arg, "--pmt=", 6) == 0) {
      pmtConfig = std::string(arg + 6);
      pmtExplicit = true;
    } else if (std::strcmp(arg, "--pmt") == 0) {
      if (i + 1 < argc) {
        pmtConfig = std::string(argv[++i]);
        pmtExplicit = true;
      } else {
        G4cout << "[WARN] --pmt flag expects a value; ignoring.\n";
      }
    } else if (std::strncmp(arg, "--opt_enable=", 13) == 0) {
      optEnableOverride = std::string(arg + 13);
    } else if (std::strcmp(arg, "--opt_enable") == 0) {
      if (i + 1 < argc) {
        optEnableOverride = std::string(argv[++i]);
      } else {
        G4cout << "[WARN] --opt_enable flag expects a value; keeping defaults.\n";
      }
    } else if (std::strcmp(arg, "--opt_dbg") == 0) {
      optDebug = true;
    } else if (std::strcmp(arg, "--quiet") == 0) {
      quiet = true;
    } else if (std::strncmp(arg, "--opt_verbose=", 14) == 0) {
      try {
        optVerbose = std::clamp(std::stoi(arg + 14), 0, 2);
      } catch (...) {
        optVerbose = 0;
        G4cout << "[WARN] Invalid value for --opt_verbose ('" << (arg + 14) << "'); using 0.\n";
      }
    } else if (std::strcmp(arg, "--opt_verbose") == 0) {
      if (i + 1 < argc) {
        try {
          optVerbose = std::clamp(std::stoi(argv[++i]), 0, 2);
        } catch (...) {
          optVerbose = 0;
          G4cout << "[WARN] Invalid value for --opt_verbose ('" << argv[i] << "'); using 0.\n";
        }
      } else {
        G4cout << "[WARN] --opt_verbose flag expects a value; keeping 0.\n";
      }
    } else if (std::strncmp(arg, "--qe_override=", 15) == 0) {
      try {
        qeOverride = std::stod(arg + 15);
      } catch (...) {
        qeOverride = std::numeric_limits<double>::quiet_NaN();
        G4cout << "[WARN] Invalid value for --qe_override ('" << (arg + 15) << "'); ignoring.\n";
      }
    } else if (std::strcmp(arg, "--qe_override") == 0) {
      if (i + 1 < argc) {
        try {
          qeOverride = std::stod(argv[++i]);
        } catch (...) {
          qeOverride = std::numeric_limits<double>::quiet_NaN();
          G4cout << "[WARN] Invalid value for --qe_override ('" << argv[i] << "'); ignoring.\n";
        }
      } else {
        G4cout << "[WARN] --qe_override flag expects a value; ignoring.\n";
      }
    } else if (std::strncmp(arg, "--qe_flat=", 10) == 0) {
      try {
        qeFlat = std::stod(arg + 10);
      } catch (...) {
        qeFlat = std::numeric_limits<double>::quiet_NaN();
        G4cout << "[WARN] Invalid value for --qe_flat ('" << (arg + 10) << "'); ignoring.\n";
      }
    } else if (std::strcmp(arg, "--qe_flat") == 0) {
      if (i + 1 < argc) {
        try {
          qeFlat = std::stod(argv[++i]);
        } catch (...) {
          qeFlat = std::numeric_limits<double>::quiet_NaN();
          G4cout << "[WARN] Invalid value for --qe_flat ('" << argv[i] << "'); ignoring.\n";
        }
      } else {
        G4cout << "[WARN] --qe_flat flag expects a value; ignoring.\n";
      }
    } else if (std::strncmp(arg, "--check_overlaps_n=", 20) == 0) {
      try {
        checkOverlapsN = std::max(0, std::stoi(arg + 20));
      } catch (...) {
        G4cout << "[WARN] Invalid value for --check_overlaps_n: " << (arg + 20) << ". Using 0.\n";
        checkOverlapsN = 0;
      }
    } else if (std::strcmp(arg, "--check_overlaps_n") == 0) {
      if (i + 1 < argc) {
        try {
          checkOverlapsN = std::max(0, std::stoi(argv[++i]));
        } catch (...) {
          G4cout << "[WARN] Invalid value for --check_overlaps_n: " << argv[i] << ". Using 0.\n";
          checkOverlapsN = 0;
        }
      } else {
        G4cout << "[WARN] --check_overlaps_n flag expects a value; keeping 0.\n";
      }
    } else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
      G4cout << "Usage: " << argv[0]
             << " [--profile=<name>] [--optics=<cfg.yaml>] [--pmt=<cfg.yaml>] [--opt_enable=list]"
             << " [--opt_dbg] [--quiet] [--opt_verbose=<0..2>] [--check_overlaps_n=<int>] [macro.mac]\n"
             << "Profiles: day1 (default), day2, day3, custom\n"
             << "Optics: defaults to detector/config/optics.yaml\n"
             << "PMT: defaults to detector/config/pmt.yaml (day2)\n"
             << "Optical processes list accepts comma-separated names: "
             << "cerenkov, abs, rayleigh, mie, boundary\n";
      return 0;
    } else if (arg[0] == '-' && arg[1] == '-') {
      G4cout << "[WARN] Unknown option '" << arg << "' ignored.\n";
    } else {
      macroArg = arg;
    }
  }

  auto toLower = [](std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
  };

  const std::string profileNorm = toLower(profile);
  const bool isDay2Profile = (profileNorm == "day2");
  const bool isDay3Profile = (profileNorm == "day3");

  G4cout << "[CFG] Profile: " << profile << G4endl;

  if (isDay2Profile && !opticsExplicit) {
    opticsConfig = "detector/config/optics.yaml";
  }

  G4cout << "[CFG] Optics config: " << opticsConfig << G4endl;
  runManager->SetUserInitialization(new DetectorConstruction(gdml, opticsConfig, checkOverlapsN, qeOverride, qeFlat));

  auto makeDefaultOptConfig = [](const std::string& prof) {
    OpticalProcessConfig cfg;
    cfg.enableCerenkov = true;
    cfg.enableAbsorption = true;
    cfg.enableRayleigh = false;
    cfg.enableBoundary = true;
    cfg.enableMie = false;
    cfg.maxPhotonsPerStep = 50;
    cfg.maxBetaChangePerStep = 10.0;
    if (prof == "day2") {
      cfg.enableCerenkov = true;
      cfg.enableAbsorption = true;
      cfg.enableBoundary = true;
      cfg.enableRayleigh = false;
      cfg.enableMie = false;
      cfg.maxPhotonsPerStep = 50;
    } else if (prof == "day3") {
      cfg.enableCerenkov = true;
      cfg.enableAbsorption = true;
      cfg.enableBoundary = true;
      cfg.enableRayleigh = true;
      cfg.enableMie = true;
      cfg.maxPhotonsPerStep = 300;
    }
    return cfg;
  };

  auto splitList = [](const std::string& list) {
    std::vector<std::string> tokens;
    std::stringstream ss(list);
    std::string item;
    while (std::getline(ss, item, ',')) {
      size_t start = 0, end = item.size();
      while (start < end && std::isspace(static_cast<unsigned char>(item[start]))) ++start;
      while (end > start && std::isspace(static_cast<unsigned char>(item[end-1]))) --end;
      if (start < end) tokens.emplace_back(item.substr(start, end - start));
    }
    return tokens;
  };

  auto applyOptOverride = [&](const std::string& overrideList, const OpticalProcessConfig& base) {
    if (overrideList.empty()) return base;
    auto tokens = splitList(overrideList);
    if (tokens.empty()) return base;

    bool onlyAll = (tokens.size() == 1 && toLower(tokens.front()) == "all");
    if (onlyAll) {
      OpticalProcessConfig cfg = base;
      cfg.enableCerenkov = cfg.enableAbsorption = cfg.enableRayleigh = cfg.enableMie = cfg.enableBoundary = true;
      return cfg;
    }
    bool onlyNone = (tokens.size() == 1 && toLower(tokens.front()) == "none");
    if (onlyNone) {
      OpticalProcessConfig cfg = base;
      cfg.enableCerenkov = cfg.enableAbsorption = cfg.enableRayleigh = cfg.enableMie = cfg.enableBoundary = false;
      return cfg;
    }

    OpticalProcessConfig cfg = base;
    cfg.enableCerenkov = cfg.enableAbsorption = cfg.enableRayleigh = cfg.enableMie = cfg.enableBoundary = false;
    bool recognised = false;
    for (const auto& rawTok : tokens) {
      const auto tok = toLower(rawTok);
      if (tok == "cherenkov" || tok == "cerenkov") {
        cfg.enableCerenkov = true; recognised = true;
      } else if (tok == "abs" || tok == "absorption") {
        cfg.enableAbsorption = true; recognised = true;
      } else if (tok == "rayleigh" || tok == "ray") {
        cfg.enableRayleigh = true; recognised = true;
      } else if (tok == "mie" || tok == "miehg") {
        cfg.enableMie = true; recognised = true;
      } else if (tok == "boundary" || tok == "surf" || tok == "surface") {
        cfg.enableBoundary = true; recognised = true;
      } else if (tok == "all") {
        cfg.enableCerenkov = cfg.enableAbsorption = cfg.enableRayleigh = cfg.enableMie = cfg.enableBoundary = true;
        recognised = true;
      } else if (tok == "none") {
        recognised = true; // leave all disabled
      } else {
        G4cout << "[WARN] Unknown --opt_enable token '" << rawTok << "' ignored.\n";
      }
    }
    if (!recognised) {
      G4cout << "[WARN] --opt_enable override contained no recognised process names; keeping defaults.\n";
      return base;
    }
    return cfg;
  };

  OpticalProcessConfig opticalCfg = makeDefaultOptConfig(profileNorm);
  opticalCfg = applyOptOverride(optEnableOverride, opticalCfg);

  auto* physicsList = new PhysicsList(opticalCfg);
  if (isDay2Profile || isDay3Profile) {
    physicsList->SetDefaultCutValue(0.1 * mm);
    G4cout << "[CFG] Applied profile '" << profile << "': default cut = 0.1 mm\n";
  } else {
    G4cout << "[CFG] Applied profile '" << profile << "'\n";
  }
  runManager->SetUserInitialization(physicsList);

  RunProfileConfig runProfile;
  runProfile.enableDigitizer = (profileNorm == "day1" || isDay2Profile || isDay3Profile);
  if (const char* envPmt = std::getenv("FLNDR_PMT_CONFIG")) {
    if (*envPmt) runProfile.pmtConfigPath = envPmt;
  }
  if (pmtExplicit) {
    runProfile.pmtConfigPath = pmtConfig;
  }
  if (const char* envOut = std::getenv("FLNDR_PMTHITS_OUT")) {
    if (*envOut) runProfile.pmtOutputPath = envOut;
  }
  if (runProfile.pmtConfigPath.empty() && (profileNorm == "day1" || isDay2Profile)) {
    runProfile.pmtConfigPath = "detector/config/pmt.yaml";
  }
  if (runProfile.pmtOutputPath.empty() && (profileNorm == "day1" || isDay2Profile)) {
    runProfile.pmtOutputPath = "docs/day4/pmt_digi.root";
  }

  if (runProfile.enableDigitizer) {
    G4cout << "[CFG] Digitizer enabled (config=" << runProfile.pmtConfigPath
           << ", out=" << runProfile.pmtOutputPath << ")\n";
  } else {
    G4cout << "[CFG] Digitizer disabled for profile '" << profile << "'\n";
  }
  G4cout << "[CFG] PMT config path: "
         << (runProfile.pmtConfigPath.empty() ? "<none>" : runProfile.pmtConfigPath)
         << G4endl;

  auto readFile = [](const std::string& path) -> std::string {
    if (path.empty()) return std::string();
    std::ifstream in(path);
    if (!in) return std::string();
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
  };

  RunManifest manifest;
  manifest.profile = profile;
  manifest.macro = macroArg.empty() ? "<interactive>" : macroArg;
  manifest.opticsPath = opticsConfig;
  manifest.opticsContents = readFile(opticsConfig);
  manifest.pmtPath = runProfile.pmtConfigPath;
  manifest.pmtContents = readFile(runProfile.pmtConfigPath);
  manifest.gitSHA = FLNDR_GIT_SHA;
  manifest.buildType = FLNDR_BUILD_TYPE;
  manifest.compiler = FLNDR_COMPILER;
  manifest.cxxFlags = FLNDR_CXX_FLAGS;
  manifest.digitizerEnabled = runProfile.enableDigitizer;
  manifest.digitizerOutput = runProfile.pmtOutputPath;
  manifest.opticsOverride = optEnableOverride;
  manifest.opticalDebug = optDebug;
  manifest.quiet = quiet;
  manifest.opticalVerboseLevel = optVerbose;
  manifest.qeScaleOverride = qeOverride;
  manifest.qeFlatOverride = qeFlat;
  SetRunManifest(std::move(manifest));

  runManager->SetUserInitialization(new ActionInitialization(rtrk, zshift, runProfile));

  // Vis + UI
  auto* visManager = new G4VisExecutive; visManager->Initialize();
  auto* UImanager = G4UImanager::GetUIpointer();

  if (quiet) {
    UImanager->ApplyCommand("/run/verbose 0");
    UImanager->ApplyCommand("/tracking/verbose 0");
  }

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
