#pragma once

#include <string>

#include <FTFP_BERT.hh>

struct OpticalProcessConfig {
  bool enableCerenkov  = true;
  bool enableAbsorption = true;
  bool enableRayleigh  = true;
  bool enableMie       = false;
  bool enableBoundary  = true;
  int  maxPhotonsPerStep = 300;
  double maxBetaChangePerStep = 10.0;
};

class PhysicsList : public FTFP_BERT {
public:
  explicit PhysicsList(const OpticalProcessConfig& cfg);
  ~PhysicsList() override = default;

  void ConstructProcess() override;

private:
  OpticalProcessConfig fConfig;
};
