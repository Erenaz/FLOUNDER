#pragma once
#include <G4String.hh>
#include <G4VUserActionInitialization.hh>

#include <string>

struct RunProfileConfig {
  bool enableDigitizer = false;
  std::string pmtConfigPath;
  std::string pmtOutputPath;
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
