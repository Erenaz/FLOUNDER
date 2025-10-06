#pragma once
#include <G4VUserActionInitialization.hh>
#include <G4String.hh>  // <-- add this

class ActionInitialization : public G4VUserActionInitialization {
public:
  ActionInitialization(const G4String& rfile, double zshift);
  ~ActionInitialization() override = default;
  void Build() const override;
private:
  G4String fRootFile;
  double   fZshift;
};
