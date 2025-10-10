#pragma once

#include <string>
#include <vector>

#include <G4UserEventAction.hh>

struct PMTDigitizerConfig {
  double qe_scale = 1.0;
  double tts_sigma_ps = 150.0;
  double elec_jitter_ps = 300.0;
  double dark_rate_hz = 0.0;
  double threshold_npe = 0.3;
  double gate_ns = 600.0;
  double gate_offset_ns = 0.0;
  std::vector<double> wavelengths_nm;
  std::vector<double> qe_curve;
};

struct PMTDigiRecord {
  int    event = -1;
  int    pmt = -1;
  double time_ns = 0.0;
  double npe = 0.0;
  int    flags = 0;
};

class PMTDigitizer : public G4UserEventAction {
public:
  PMTDigitizer(std::string configPath,
               std::string outputPath = "docs/day4/pmt_digi.root");
  ~PMTDigitizer() override;

  void BeginOfEventAction(const G4Event*) override;
  void EndOfEventAction(const G4Event*) override;

  static PMTDigitizerConfig LoadConfig(const std::string& path);

private:
  void ensureInitialized();
  void ensureOutput();
  void cachePMTs();
  void digitizeEvent(const G4Event*);
  double sampleQE(double wavelength_nm) const;

  std::string configPath_;
  std::string outputPath_;
  PMTDigitizerConfig cfg_;

  bool cfgLoaded_ = false;
  bool outputOpen_ = false;
  bool geometryCached_ = false;

  int hitsCollectionId_ = -1;
  double sigma_ns_ = 0.0;

  std::vector<int> allPmts_;

  struct Writer;
  Writer* writer_ = nullptr;
};
