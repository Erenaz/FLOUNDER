#pragma once
#include <G4String.hh>
#include <G4VUserActionInitialization.hh>

#include <string>
#include <optional>

struct RunProfileConfig {
  bool enableDigitizer = false;
  bool enableTTS = true;
  bool enableJitter = true;
  std::string gateMode = "standard";
  std::optional<double> gateNsOverride;
  std::string pmtConfigPath;
  std::string pmtOutputPath;
  std::optional<double> qeFlatOverride;
  std::optional<double> qeScaleFactor;
  std::optional<double> thresholdOverride;
};

class ActionInitialization : public G4VUserActionInitialization {
public:
  ActionInitialization(const G4String& rfile,
                       double zshift,
                       RunProfileConfig profile);
  ~ActionInitialization() override = default;
  void Build() const override;

private:
  G4String fRootFile;
  double   fZshift;
  RunProfileConfig fProfile;
};
