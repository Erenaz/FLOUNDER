#pragma once

#include <string>
#include <vector>
#include <optional>

#include <G4UserEventAction.hh>

struct PMTDigitizerConfig {
  double qe_scale = 1.0;
  double tts_sigma_ps = 150.0;
  double tts_sigma_ns = 0.0;
  std::string tts_units = "sigma_ps";
  double elec_jitter_ps = 300.0;
  double jitter_sigma_ns = 0.0;
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
               std::string outputPath = "docs/day4/pmt_digi.root",
               std::optional<double> qeFlatOverride = std::nullopt,
               std::optional<double> qeScaleFactor = std::nullopt,
               std::optional<double> thresholdOverride = std::nullopt,
               bool enableTTS = true,
               bool enableJitter = true,
               std::string gateMode = "standard",
               std::optional<double> gateNsOverride = std::nullopt);
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
  void emitFinalSummary() const;

  std::string configPath_;
  std::string outputPath_;
  std::optional<double> qeFlatOverride_;
  std::optional<double> qeScaleFactor_;
  std::optional<double> thresholdOverride_;
  PMTDigitizerConfig cfg_;

  bool cfgLoaded_ = false;
  bool outputOpen_ = false;
  bool geometryCached_ = false;
  bool loggedEffectiveQE_ = false;
  bool storeAllSamples_ = false;
  bool loggedTimingSigma_ = false;
  bool loggedEffectiveCfg_ = false;
  bool loggedGateConfig_ = false;
  bool enableTTS_ = true;
  bool enableJitter_ = true;
  std::string gateMode_ = "standard";
  std::optional<double> gateNsOverride_;
  double gateWindowNs_ = 0.0;
  bool gateModeStandard_ = true;
  bool gateModeCentered_ = false;
  bool gateModeOff_ = false;

  int hitsCollectionId_ = -1;
  double sigma_ns_ = 0.0;

  std::vector<int> allPmts_;

  struct Writer;
  Writer* writer_ = nullptr;

  unsigned long long eventsProcessed_ = 0;
  double totalPEs_ = 0.0;
};
