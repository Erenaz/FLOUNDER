#include "PMTDigitizer.hh"

#include "PMTSD.hh"
#include "PhotonBudget.hh"
#include "RunManifest.hh"

#include <yaml-cpp/yaml.h>

#include <G4Event.hh>
#include <G4HCofThisEvent.hh>
#include <G4HCtable.hh>
#include <G4PhysicalVolumeStore.hh>
#include <G4Poisson.hh>
#include <G4RunManager.hh>
#include <G4SDManager.hh>
#include <G4SystemOfUnits.hh>
#include <G4ios.hh>

#include "Randomize.hh"

#include "TFile.h"
#include "TTree.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

double hypot_ns(double tts_ps, double jitter_ps) {
  const double tts_ns = tts_ps * 1e-3;
  const double jit_ns = jitter_ps * 1e-3;
  return std::hypot(tts_ns, jit_ns);
}

std::string to_lower_copy(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  return s;
}

YAML::Node find_child_ci(const YAML::Node& parent, const std::string& key) {
  if (!parent || !parent.IsMap()) return YAML::Node();
  std::string target = to_lower_copy(key);
  for (auto it = parent.begin(); it != parent.end(); ++it) {
    if (!it->first.IsScalar()) continue;
    if (to_lower_copy(it->first.Scalar()) == target) {
      return it->second;
    }
  }
  return YAML::Node();
}

YAML::Node find_child_ci(const YAML::Node& parent, std::initializer_list<const char*> keys) {
  for (const char* key : keys) {
    if (!key) continue;
    auto node = find_child_ci(parent, key);
    if (node) return node;
  }
  return YAML::Node();
}

std::vector<double> load_vector_ci(const YAML::Node& parent,
                                   std::initializer_list<const char*> keys,
                                   const std::string& context) {
  auto node = find_child_ci(parent, keys);
  if (!node || !node.IsSequence()) {
    std::ostringstream oss;
    bool first = true;
    for (const char* k : keys) {
      if (!first) oss << "/";
      first = false;
      oss << k;
    }
    throw std::runtime_error("PMT digitizer config missing sequence '" + oss.str() +
                             "' in context '" + context + "'.");
  }
  std::vector<double> result;
  result.reserve(node.size());
  for (const auto& v : node) {
    if (!v.IsScalar()) {
      throw std::runtime_error("Non-scalar entry in sequence for " + context);
    }
    const std::string s = v.Scalar();
    result.push_back(std::stod(s));
  }
  return result;
}

double scalar_to_double(const YAML::Node& node, const char* name) {
  if (!node || node.IsNull() || !node.IsDefined()) {
    throw std::runtime_error(std::string("Missing scalar for ") + name + " in PMT digitizer config");
  }
  if (!node.IsScalar()) {
    throw std::runtime_error(std::string("Expected scalar for ") + name + " in PMT digitizer config");
  }
  try {
    return node.as<double>();
  } catch (const YAML::BadConversion&) {
    return std::stod(node.Scalar());
  }
}

double mean_qe_in_window(const std::vector<double>& wavelengths,
                         const std::vector<double>& qe,
                         double lo_nm, double hi_nm) {
  if (wavelengths.size() != qe.size() || wavelengths.empty()) return 0.0;
  double area = 0.0;
  double width = 0.0;
  for (size_t i = 0; i + 1 < wavelengths.size(); ++i) {
    const double x0 = wavelengths[i];
    const double x1 = wavelengths[i + 1];
    const double y0 = qe[i];
    const double y1 = qe[i + 1];
    const double L = std::max(std::min(x0, x1), lo_nm);
    const double R = std::min(std::max(x0, x1), hi_nm);
    if (L >= R) continue;
    const double t0 = (L - x0) / (x1 - x0);
    const double t1 = (R - x0) / (x1 - x0);
    const double qL = y0 + t0 * (y1 - y0);
    const double qR = y0 + t1 * (y1 - y0);
    const double segmentWidth = R - L;
    area += 0.5 * (qL + qR) * segmentWidth;
    width += segmentWidth;
  }
  return width > 0.0 ? area / width : 0.0;
}

struct Sample {
  double time_ns = 0.0;
  int flags = 0;
};

} // namespace

struct PMTDigitizer::Writer {
  TFile* file = nullptr;
  TTree* tree = nullptr;
  int    b_event = 0;
  int    b_pmt = 0;
  double b_time = 0.0;
  double b_npe = 0.0;
  int    b_flags = 0;

  ~Writer() {
    if (file) {
      file->cd();
      tree->Write();
      file->Write();
      file->Close();
      delete file;
    }
  }

  void open(const std::string& path) {
    if (file) return;
    auto dir = std::filesystem::path(path).parent_path();
    if (!dir.empty()) {
      std::filesystem::create_directories(dir);
    }
    file = TFile::Open(path.c_str(), "RECREATE");
    if (!file || file->IsZombie()) {
      throw std::runtime_error("Failed to open PMT digitizer output file: " + path);
    }
    tree = new TTree("hits", "Digitized PMT hits");
    tree->Branch("event", &b_event);
    tree->Branch("pmt",   &b_pmt);
    tree->Branch("t_ns",  &b_time);
    tree->Branch("npe",   &b_npe);
    tree->Branch("flags", &b_flags);
    tree->SetDirectory(file);
    RegisterOutputFile(file);
  }

  void fill(const PMTDigiRecord& rec) {
    b_event = rec.event;
    b_pmt   = rec.pmt;
    b_time  = rec.time_ns;
    b_npe   = rec.npe;
    b_flags = rec.flags;
    tree->Fill();
  }
};

PMTDigitizerConfig PMTDigitizer::LoadConfig(const std::string& path) {
  try {
    YAML::Node root = YAML::LoadFile(path);
    PMTDigitizerConfig cfg;
    if (auto node = find_child_ci(root, {"qe_scale", "QE_scale", "QE_SCALE"}); node && node.IsDefined() && !node.IsNull()) cfg.qe_scale = scalar_to_double(node, "QE_scale");
    if (auto node = find_child_ci(root, {"tts_sigma_ps", "TTS_sigma_ps", "TTS_sigma_PS"}); node && node.IsDefined() && !node.IsNull()) cfg.tts_sigma_ps = scalar_to_double(node, "TTS_sigma_ps");
    if (auto node = find_child_ci(root, {"elec_jitter_ps", "ELEC_JITTER_PS", "elec_jitter"}); node && node.IsDefined() && !node.IsNull()) cfg.elec_jitter_ps = scalar_to_double(node, "elec_jitter_ps");
    if (auto node = find_child_ci(root, {"dark_rate_hz", "dark_rate_Hz", "dark_rate", "DARK_RATE_HZ"}); node && node.IsDefined() && !node.IsNull()) cfg.dark_rate_hz = scalar_to_double(node, "dark_rate_hz");
    if (auto node = find_child_ci(root, {"threshold_npe", "THRESHOLD_NPE", "threshold"}); node && node.IsDefined() && !node.IsNull()) cfg.threshold_npe = scalar_to_double(node, "threshold_npe");
    if (auto node = find_child_ci(root, {"gate_ns", "GATE_NS"}); node && node.IsDefined() && !node.IsNull()) cfg.gate_ns = scalar_to_double(node, "gate_ns");
    if (auto node = find_child_ci(root, {"gate_offset_ns", "GATE_OFFSET_NS"}); node && node.IsDefined() && !node.IsNull()) cfg.gate_offset_ns = scalar_to_double(node, "gate_offset_ns");
    if (find_child_ci(root, {"wavelength_nm", "WAVELENGTH_NM"})) {
      cfg.wavelengths_nm = load_vector_ci(root, {"wavelength_nm", "WAVELENGTH_NM"}, "wavelength_nm");
    }
    const std::initializer_list<const char*> qeKeys = {"QE_curve", "QE", "PMT_QE", "EFFICIENCY"};
    bool qeFound = false;
    for (const char* key : qeKeys) {
      auto node = find_child_ci(root, {key});
      if (node && node.IsSequence()) {
        cfg.qe_curve = load_vector_ci(root, {key}, key);
        qeFound = true;
        break;
      }
    }
    if (!qeFound) {
      throw std::runtime_error("PMT digitizer config '" + path + "' missing QE curve (expected one of QE_curve/QE/PMT_QE/EFFICIENCY).");
    }
    if (!cfg.wavelengths_nm.empty() && cfg.qe_curve.size() != cfg.wavelengths_nm.size()) {
      throw std::runtime_error("PMT digitizer config '" + path + "': qe list length must match wavelength_nm length.");
    }
    return cfg;
  } catch (const YAML::BadFile&) {
    throw std::runtime_error("PMTDigitizer: cannot open config '" + path + "'");
  } catch (const YAML::Exception& ex) {
    throw std::runtime_error("PMTDigitizer: error parsing '" + path + "': " + ex.what());
  }
}

PMTDigitizer::PMTDigitizer(std::string configPath, std::string outputPath)
  : configPath_(std::move(configPath)),
    outputPath_(std::move(outputPath)) {}

PMTDigitizer::~PMTDigitizer() {
  delete writer_;
}

void PMTDigitizer::ensureInitialized() {
  if (!cfgLoaded_) {
    cfg_ = LoadConfig(configPath_);
    cfg_.qe_scale = std::clamp(cfg_.qe_scale, 0.0, 1.0);
    if (cfg_.threshold_npe < 0.0) cfg_.threshold_npe = 0.0;
    if (cfg_.gate_ns < 0.0) cfg_.gate_ns = 0.0;
    if (cfg_.wavelengths_nm.empty() || cfg_.qe_curve.empty()) {
      throw std::runtime_error("PMTDigitizer: config must provide wavelength_nm and qe arrays for QE sampling.");
    }
    std::vector<std::pair<double,double>> pairs;
    pairs.reserve(cfg_.wavelengths_nm.size());
    for (size_t i = 0; i < cfg_.wavelengths_nm.size(); ++i) {
      pairs.emplace_back(cfg_.wavelengths_nm[i], cfg_.qe_curve[i]);
    }
    std::sort(pairs.begin(), pairs.end(),
              [](const auto& a, const auto& b){ return a.first < b.first; });
    cfg_.wavelengths_nm.clear(); cfg_.qe_curve.clear();
    for (const auto& p : pairs) {
      cfg_.wavelengths_nm.push_back(p.first);
      cfg_.qe_curve.push_back(std::clamp(p.second, 0.0, 1.0));
    }
    sigma_ns_ = hypot_ns(cfg_.tts_sigma_ps, cfg_.elec_jitter_ps);
    cfgLoaded_ = true;
    const double meanQE = mean_qe_in_window(cfg_.wavelengths_nm, cfg_.qe_curve, 400.0, 450.0);
    const double peakQE = cfg_.qe_curve.empty() ? 0.0 : *std::max_element(cfg_.qe_curve.begin(), cfg_.qe_curve.end());
    G4cout << "[PMTDigi] Loaded config '" << configPath_ << "'"
           << " qe_scale=" << cfg_.qe_scale
           << " sigma_ns=" << sigma_ns_
           << " dark_rate=" << cfg_.dark_rate_hz << " Hz"
           << " threshold=" << cfg_.threshold_npe << " PE"
           << " gate=" << cfg_.gate_ns << " ns"
           << " qe_points=" << cfg_.wavelengths_nm.size()
           << " meanQE(400-450nm)=" << std::fixed << std::setprecision(2) << meanQE * cfg_.qe_scale * 100.0 << "%"
           << " peakQE=" << peakQE * cfg_.qe_scale * 100.0 << "%"
           << G4endl;
  }

  if (hitsCollectionId_ < 0) {
    auto* sdm = G4SDManager::GetSDMpointer();
    const char* candidates[] = {"PMTSD/OpticalHits", "PMTSD/PMTHits"};
    for (const char* name : candidates) {
      if (!name) continue;
      int id = sdm->GetCollectionID(name);
      if (id >= 0) {
        hitsCollectionId_ = id;
        break;
      }
    }
    if (hitsCollectionId_ < 0) {
      auto* hcTable = sdm->GetHCtable();
      if (hcTable && hcTable->entries() > 0) {
        for (int i = 0; i < hcTable->entries(); ++i) {
          G4cout << "[HCE] idx=" << i
                 << " name=" << hcTable->GetSDname(i)
                 << "/" << hcTable->GetHCname(i)
                 << G4endl;
        }
      } else {
        G4cout << "[HCE] (no registered collections)\n";
      }
      std::ostringstream oss;
      oss << "PMTDigitizer: missing PMT hits collection. Tried: ";
      for (const char* name : candidates) {
        if (name) oss << name << " ";
      }
      G4Exception("PMTDigitizer", "MissingHCID", FatalException, oss.str().c_str());
      return;
    }
  }

  cachePMTs();
}

void PMTDigitizer::ensureOutput() {
  if (outputOpen_) return;
  if (!writer_) writer_ = new Writer();
  writer_->open(outputPath_);
  outputOpen_ = true;
}

void PMTDigitizer::cachePMTs() {
  if (geometryCached_) return;
  geometryCached_ = true;
  std::unordered_set<int> unique;
  auto* store = G4PhysicalVolumeStore::GetInstance();
  if (store) {
    for (auto* pv : *store) {
      if (!pv) continue;
      if (pv->GetName() == "PMT") {
        unique.insert(pv->GetCopyNo());
      }
    }
  }
  allPmts_.assign(unique.begin(), unique.end());
  std::sort(allPmts_.begin(), allPmts_.end());
  G4cout << "[PMTDigi] Cached " << allPmts_.size()
         << " PMT placements for dark noise sampling." << G4endl;
}

void PMTDigitizer::BeginOfEventAction(const G4Event* event) {
  ensureInitialized();
  if (GetRunManifest().opticalDebug && event && event->GetEventID() == 0) {
    G4cout << "[OPT_DBG] Event 0: digitizer boundary sampling enabled (limited output)" << G4endl;
  }
}

void PMTDigitizer::EndOfEventAction(const G4Event* event) {
  try {
    digitizeEvent(event);
  } catch (const std::exception& ex) {
    G4Exception("PMTDigitizer", "DigitizeFail", FatalException, ex.what());
  }
}

void PMTDigitizer::digitizeEvent(const G4Event* event) {
  ensureOutput();

  if (!event) {
    G4cout << "[HCE] (no event)" << G4endl;
    return;
  }

  auto* hcContainer = event->GetHCofThisEvent();
  if (!hcContainer) {
    G4cout << "[HCE] (null)" << G4endl;
    return;
  }
  const G4int nCollections = hcContainer->GetNumberOfCollections();
  for (G4int i = 0; i < nCollections; ++i) {
    auto* hc = hcContainer->GetHC(i);
    const G4String name = hc ? hc->GetName() : "(null)";
    const G4int size = hc ? hc->GetSize() : 0;
    G4cout << "[HCE] idx=" << i << " name=" << name << " size=" << size << G4endl;
  }

  auto* raw = hcContainer->GetHC(hitsCollectionId_);
  if (!raw) {
    std::ostringstream oss;
    oss << "PMTDigitizer: event " << event->GetEventID()
        << " missing hits collection id=" << hitsCollectionId_;
    G4Exception("PMTDigitizer", "MissingHC", FatalException, oss.str().c_str());
    return;
  }

  auto* hits = static_cast<PMTHitsCollection*>(raw);
  const size_t nHits = hits->entries();

  std::unordered_map<int, std::vector<Sample>> perPMT;
  perPMT.reserve(nHits + allPmts_.size());

  std::unordered_set<int> pmtsSeen;

  const double t0_ns = PrimaryInfo::T0ns();
  const double gateStart = t0_ns + cfg_.gate_offset_ns;
  const double gateEnd   = gateStart + cfg_.gate_ns;
  const bool useGate     = cfg_.gate_ns > 0.0;

  std::size_t rawCount = 0;
  std::size_t keptCount = 0;
  std::size_t darkCount = 0;

  for (size_t i = 0; i < nHits; ++i) {
    const PMTHit* hit = (*hits)[i];
    if (!hit) continue;
    ++rawCount;

    double lambda_nm = hit->wavelength_nm;
    double prob = cfg_.qe_scale * sampleQE(lambda_nm);
    prob = std::clamp(prob, 0.0, 1.0);
    if (prob <= 0.0) continue;
    if (G4UniformRand() > prob) continue;
    ++keptCount;

    double t_ns = hit->time / ns;
    if (sigma_ns_ > 0.0) {
      t_ns += G4RandGauss::shoot(0.0, sigma_ns_);
    }
    if (useGate) {
      if (t_ns < gateStart || t_ns > gateEnd) continue;
    }

    auto& vec = perPMT[hit->pmt_id];
    vec.push_back({t_ns, hit->flags});
    pmtsSeen.insert(hit->pmt_id);
  }

  if (cfg_.dark_rate_hz > 0.0 && cfg_.gate_ns > 0.0) {
    const double gateWidth_s = cfg_.gate_ns * 1e-9;
    const double mean = cfg_.dark_rate_hz * gateWidth_s;
    std::vector<int> dynamicTargets;
    const std::vector<int>* targets = &allPmts_;
    if (allPmts_.empty()) {
      dynamicTargets.assign(pmtsSeen.begin(), pmtsSeen.end());
      targets = &dynamicTargets;
    }
    for (int pmt : *targets) {
      const int k = G4Poisson(mean);
      if (k <= 0) continue;
      auto& vec = perPMT[pmt];
      vec.reserve(vec.size() + static_cast<size_t>(k));
      for (int i = 0; i < k; ++i) {
        const double t = gateStart + G4UniformRand() * cfg_.gate_ns;
        vec.push_back({t, 0x1}); // flag bit 0x1 => dark
        ++darkCount;
      }
    }
  }

  std::vector<PMTDigiRecord> records;
  records.reserve(perPMT.size());
  const int eventId = event->GetEventID();

  for (auto& kv : perPMT) {
    auto& samples = kv.second;
    if (samples.empty()) continue;

    const double npe = static_cast<double>(samples.size());
    if (npe < cfg_.threshold_npe) continue;

    const auto minIt = std::min_element(
        samples.begin(), samples.end(),
        [](const Sample& a, const Sample& b){ return a.time_ns < b.time_ns; });

    int flagMask = 0;
    for (const auto& s : samples) {
      flagMask |= s.flags;
    }
    if (npe >= 10.0) {
      flagMask |= 0x4; // saturated
    }

    records.push_back({eventId, kv.first, minIt->time_ns, npe, flagMask});
  }

  for (const auto& rec : records) {
    writer_->fill(rec);
  }

  static bool printedSample = false;
  if (!printedSample) {
    G4cout << "[PMTDigi] sample evt0 -> raw=" << rawCount
           << " kept=" << keptCount
           << " dark=" << darkCount << G4endl;
    printedSample = true;
  }

  G4cout << "[PMTDigi] evt=" << eventId
         << " raw=" << rawCount
         << " kept=" << keptCount
         << " dark=" << darkCount
         << " out=" << records.size()
         << G4endl;
}

double PMTDigitizer::sampleQE(double wavelength_nm) const {
  if (cfg_.wavelengths_nm.empty()) return 0.0;
  if (wavelength_nm <= 0.0) return cfg_.qe_curve.front();
  if (cfg_.wavelengths_nm.size() == 1) return cfg_.qe_curve.front();

  if (wavelength_nm <= cfg_.wavelengths_nm.front()) {
    return cfg_.qe_curve.front();
  }
  if (wavelength_nm >= cfg_.wavelengths_nm.back()) {
    return cfg_.qe_curve.back();
  }

  auto upper = std::upper_bound(cfg_.wavelengths_nm.begin(), cfg_.wavelengths_nm.end(), wavelength_nm);
  size_t idx = static_cast<size_t>(upper - cfg_.wavelengths_nm.begin());
  size_t i0 = idx - 1;
  size_t i1 = idx;
  double x0 = cfg_.wavelengths_nm[i0];
  double x1 = cfg_.wavelengths_nm[i1];
  double y0 = cfg_.qe_curve[i0];
  double y1 = cfg_.qe_curve[i1];
  if (x1 == x0) return y0;
  double t = (wavelength_nm - x0) / (x1 - x0);
  return y0 + t * (y1 - y0);
}
