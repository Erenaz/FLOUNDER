#include "OpticalProperties.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4MaterialPropertyVector.hh"
#include "G4NistManager.hh"
#include "G4OpticalSurface.hh"
#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"

namespace {

using VecD = std::vector<double>;

std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

YAML::Node find_child_ci(const YAML::Node& parent, const std::string& key) {
  if (!parent || !parent.IsMap()) return YAML::Node();
  auto target = to_lower(key);
  for (auto it = parent.begin(); it != parent.end(); ++it) {
    if (!it->first.IsScalar()) continue;
    std::string current = it->first.Scalar();
    if (to_lower(current) == target) {
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

VecD load_double_vector(const YAML::Node& parent, std::initializer_list<const char*> keys,
                        const std::string& context) {
  auto node = find_child_ci(parent, keys);
  if (!node || !node.IsSequence()) {
    std::ostringstream oss;
    oss << "Missing or non-sequence key [";
    bool first = true;
    for (const char* key : keys) {
      if (!first) oss << "/";
      first = false;
      oss << key;
    }
    oss << "] under '" << context << "' in optics YAML.";
    throw std::runtime_error(oss.str());
  }
  VecD out;
  out.reserve(node.size());
  for (const auto& v : node) {
    if (!v.IsScalar()) {
      throw std::runtime_error("Non-scalar entry in sequence for '" + context + "'.");
    }
    const std::string s = v.Scalar();
    out.push_back(std::stod(s));
  }
  return out;
}

std::string get_string_ci(const YAML::Node& parent, std::initializer_list<const char*> keys,
                          const std::string& def = std::string()) {
  auto node = find_child_ci(parent, keys);
  if (node && node.IsScalar()) {
    return node.as<std::string>();
  }
  return def;
}

double get_double_ci(const YAML::Node& parent, std::initializer_list<const char*> keys,
                     double def, bool* found = nullptr) {
  auto node = find_child_ci(parent, keys);
  if (node && !node.IsNull()) {
    if (found) *found = true;
    try {
      return node.as<double>();
    } catch (const YAML::BadConversion&) {
      if (!node.IsScalar()) {
        G4cout << "[Optics] Key requested: ";
        for (const char* k : keys) {
          G4cout << k << " ";
        }
        G4cout << " type=" << node.Type() << G4endl;
        std::ostringstream oss;
        oss << "Non-scalar value for key (";
        bool first = true;
        for (const char* k : keys) {
          if (!first) oss << "/";
          first = false;
          oss << k;
        }
        oss << ") in optics YAML.";
        throw std::runtime_error(oss.str());
      }
      return std::stod(node.Scalar());
    }
  }
  if (found) *found = false;
  return def;
}

template <typename T>
void reorder_in_place(std::vector<T>& data, const std::vector<size_t>& order) {
  std::vector<T> copy;
  copy.reserve(order.size());
  for (size_t idx : order) {
    copy.push_back(data.at(idx));
  }
  data.swap(copy);
}

std::vector<size_t> sort_order(const VecD& wavelengths) {
  std::vector<size_t> order(wavelengths.size());
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(),
            [&](size_t a, size_t b) { return wavelengths[a] < wavelengths[b]; });
  return order;
}

std::vector<G4double> WavelengthsToPhotonEnergy(const std::vector<double>& wavelengthsNm) {
  std::vector<G4double> energies;
  energies.reserve(wavelengthsNm.size());
  for (auto it = wavelengthsNm.rbegin(); it != wavelengthsNm.rend(); ++it) {
    const double lambda_nm = *it;
    if (lambda_nm <= 0.0) {
      throw std::runtime_error("Non-positive wavelength provided for photocathode RINDEX grid.");
    }
    const double energyVal = (h_Planck * c_light) / (lambda_nm * nm);
    energies.push_back(energyVal);
  }
  return energies;
}

G4OpticalSurfaceModel parse_model(const std::string& modelStr) {
  static const std::unordered_map<std::string, G4OpticalSurfaceModel> modelMap = {
      {"glisur", glisur},
      {"unified", unified},
      {"lut", LUT},
      {"davis", DAVIS},
      {"dichroic", dichroic},
  };
  auto key = to_lower(modelStr);
  auto it = modelMap.find(key);
  if (it == modelMap.end()) {
    throw std::runtime_error("Unsupported optical surface model '" + modelStr + "'");
  }
  return it->second;
}

G4OpticalSurfaceFinish parse_finish(const std::string& finishStr) {
  static const std::unordered_map<std::string, G4OpticalSurfaceFinish> finishMap = {
      {"polished", polished},
      {"polishedfrontpainted", polishedfrontpainted},
      {"polishedbackpainted", polishedbackpainted},
      {"ground", ground},
      {"groundfrontpainted", groundfrontpainted},
      {"groundbackpainted", groundbackpainted},
      {"polishedlumirrorair", polishedlumirrorair},
      {"polishedlumirrorglue", polishedlumirrorglue},
      {"polishedair", polishedair},
      {"polishedteflonair", polishedteflonair},
      {"polishedtioair", polishedtioair},
      {"polishedtyvekair", polishedtyvekair},
      {"polishedvm2000air", polishedvm2000air},
      {"polishedvm2000glue", polishedvm2000glue},
      {"etchedlumirrorair", etchedlumirrorair},
      {"etchedlumirrorglue", etchedlumirrorglue},
      {"etchedair", etchedair},
      {"etchedteflonair", etchedteflonair},
      {"etchedtioair", etchedtioair},
      {"etchedtyvekair", etchedtyvekair},
      {"etchedvm2000air", etchedvm2000air},
      {"etchedvm2000glue", etchedvm2000glue},
      {"groundlumirrorair", groundlumirrorair},
      {"groundlumirrorglue", groundlumirrorglue},
      {"groundair", groundair},
      {"groundteflonair", groundteflonair},
      {"groundtioair", groundtioair},
      {"groundtyvekair", groundtyvekair},
      {"groundvm2000air", groundvm2000air},
      {"groundvm2000glue", groundvm2000glue},
      {"rough_lut", Rough_LUT},
      {"roughteflon_lut", RoughTeflon_LUT},
      {"roughesr_lut", RoughESR_LUT},
      {"roughesrgrease_lut", RoughESRGrease_LUT},
      {"polished_lut", Polished_LUT},
      {"polishedteflon_lut", PolishedTeflon_LUT},
      {"polishedesr_lut", PolishedESR_LUT},
      {"polishedesrgrease_lut", PolishedESRGrease_LUT},
      {"detector_lut", Detector_LUT},
  };
  auto key = to_lower(finishStr);
  auto it = finishMap.find(key);
  if (it == finishMap.end()) {
    throw std::runtime_error("Unsupported optical surface finish '" + finishStr + "'");
  }
  return it->second;
}

G4SurfaceType parse_type(const std::string& typeStr) {
  static const std::unordered_map<std::string, G4SurfaceType> typeMap = {
      {"dielectric_metal", dielectric_metal},
      {"dielectric_dielectric", dielectric_dielectric},
      {"dielectric_lut", dielectric_LUT},
      {"dielectric_lutdavis", dielectric_LUTDAVIS},
      {"firsov", firsov},
      {"x_ray", x_ray},
  };
  auto key = to_lower(typeStr);
  auto it = typeMap.find(key);
  if (it == typeMap.end()) {
    throw std::runtime_error("Unsupported optical surface type '" + typeStr + "'");
  }
  return it->second;
}

double clip_unit_interval(double value) {
  if (value < 0.0) return 0.0;
  if (value > 1.0) return 1.0;
  return value;
}

VecD resample_to_grid(const VecD& sourceLambda,
                      const VecD& sourceValues,
                      const VecD& targetLambda) {
  if (targetLambda.empty()) return sourceValues;
  if (sourceValues.empty()) return VecD(targetLambda.size(), 0.0);

  VecD srcX = sourceLambda;
  if (srcX.size() != sourceValues.size()) {
    srcX.clear();
  }

  if (srcX.empty()) {
    if (sourceValues.size() == targetLambda.size()) {
      return sourceValues;
    }
    if (sourceValues.size() == 1) {
      return VecD(targetLambda.size(), sourceValues.front());
    }
    const double minX = targetLambda.front();
    const double maxX = targetLambda.back();
    const size_t count = sourceValues.size();
    srcX.resize(count);
    const double step = (count > 1) ? (maxX - minX) / static_cast<double>(count - 1) : 0.0;
    for (size_t i = 0; i < count; ++i) {
      srcX[i] = minX + step * i;
    }
  }

  std::vector<std::pair<double, double>> samples;
  samples.reserve(sourceValues.size());
  for (size_t i = 0; i < sourceValues.size(); ++i) {
    samples.emplace_back(srcX[i], sourceValues[i]);
  }
  std::sort(samples.begin(), samples.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  samples.erase(std::unique(samples.begin(), samples.end(),
                            [](const auto& a, const auto& b) {
                              return std::abs(a.first - b.first) < 1e-9;
                            }),
                samples.end());

  auto interpolate = [&](double x) {
    if (samples.size() == 1) return samples.front().second;
    if (x <= samples.front().first) return samples.front().second;
    if (x >= samples.back().first) return samples.back().second;
    auto upper = std::upper_bound(samples.begin(), samples.end(), x,
                                  [](double value, const auto& sample) {
                                    return value < sample.first;
                                  });
    size_t idx = static_cast<size_t>(upper - samples.begin());
    if (idx == 0) return samples.front().second;
    const double x1 = samples[idx].first;
    const double y1 = samples[idx].second;
    const double x0 = samples[idx - 1].first;
    const double y0 = samples[idx - 1].second;
    if (std::abs(x1 - x0) < 1e-9) return y0;
    const double t = (x - x0) / (x1 - x0);
    return y0 + t * (y1 - y0);
  };

  VecD out;
  out.reserve(targetLambda.size());
  for (double x : targetLambda) {
    out.push_back(interpolate(x));
  }
  return out;
}

void LogQESamples(const VecD& wavelengths, const VecD& qeFraction) {
  const size_t count = std::min({static_cast<size_t>(5), wavelengths.size(), qeFraction.size()});
  if (count == 0) return;
  std::ios::fmtflags oldFlags = G4cout.flags();
  std::streamsize oldPrecision = G4cout.precision();
  G4cout.setf(std::ios::fixed);
  G4cout << std::setprecision(3);
  G4cout << "[PMT.QE] sample:";
  for (size_t i = 0; i < count; ++i) {
    G4cout << " (" << wavelengths[i] << "," << qeFraction[i] << ")";
  }
  G4cout << G4endl;
  G4cout.flags(oldFlags);
  G4cout.precision(oldPrecision);
}

PMTOpticsSummaryYaml compute_pmt_summary(const VecD& wavelengths,
                                         const VecD& qeFraction) {
  PMTOpticsSummaryYaml summary;
  if (wavelengths.empty()) return summary;
  summary.lambdaMinNm = wavelengths.front();
  summary.lambdaMaxNm = wavelengths.back();
  summary.npoints = qeFraction.size();
  summary.peakQE = qeFraction.empty() ? 0.0 : *std::max_element(qeFraction.begin(), qeFraction.end());

  constexpr double A = 400.0;
  constexpr double B = 450.0;
  if (A >= B) return summary;

  const size_t n = wavelengths.size();
  double area = 0.0;
  double width = 0.0;

  auto interpolate = [&](size_t i, double lambda) {
    double x0 = wavelengths[i];
    double x1 = wavelengths[i + 1];
    if (std::abs(x1 - x0) < 1e-9) return qeFraction[i];
    double y0 = qeFraction[i];
    double y1 = qeFraction[i + 1];
    double t = (lambda - x0) / (x1 - x0);
    return y0 + t * (y1 - y0);
  };

  for (size_t i = 0; i + 1 < n; ++i) {
    double L = std::max(std::min(wavelengths[i], wavelengths[i + 1]), A);
    double R = std::min(std::max(wavelengths[i], wavelengths[i + 1]), B);
    if (L >= R) continue;
    double qL = (L == wavelengths[i]) ? qeFraction[i] : interpolate(i, L);
    double qR = (R == wavelengths[i + 1]) ? qeFraction[i + 1] : interpolate(i, R);
    area += 0.5 * (qL + qR) * (R - L);
    width += (R - L);
  }
  if (width > 0.0) summary.meanQE400to450 = area / width;
  return summary;
}

WaterOpticsSummaryYaml compute_water_summary(const VecD& wavelengths,
                                             const VecD& rindex,
                                             const VecD& absorptionMm,
                                             const VecD& scatteringMm) {
  WaterOpticsSummaryYaml summary;
  if (wavelengths.empty()) return summary;
  summary.lambdaMinNm = wavelengths.front();
  summary.lambdaMaxNm = wavelengths.back();
  summary.npoints = wavelengths.size();
  auto [rmin, rmax] = std::minmax_element(rindex.begin(), rindex.end());
  auto [amin, amax] = std::minmax_element(absorptionMm.begin(), absorptionMm.end());
  auto [smin, smax] = std::minmax_element(scatteringMm.begin(), scatteringMm.end());
  summary.rindexMin = *rmin;
  summary.rindexMax = *rmax;
  summary.absorptionMinMm = *amin;
  summary.absorptionMaxMm = *amax;
  summary.scatteringMinMm = *smin;
  summary.scatteringMaxMm = *smax;
  return summary;
}

} // namespace

OpticalPropertiesResult OpticalProperties::LoadFromYaml(const std::string& path,
                                                         double qeOverride) {
  YAML::Node root = YAML::LoadFile(path);

  if (!root || !root.IsMap()) {
    throw std::runtime_error("Optics YAML '" + path + "' is empty or not a map.");
  }

  auto wavelengths = load_double_vector(root, {"wavelength_nm", "WAVELENGTH_NM"}, "root");
  if (wavelengths.size() < 2) {
    throw std::runtime_error("Optics YAML '" + path + "' must provide >= 2 wavelength points.");
  }

  auto order = sort_order(wavelengths);
  reorder_in_place(wavelengths, order);

  auto waterNode = find_child_ci(root, "water");
  if (!waterNode || !waterNode.IsMap()) {
    throw std::runtime_error("Optics YAML '" + path + "' missing 'water' section.");
  }
  for (auto it = waterNode.begin(); it != waterNode.end(); ++it) {
    if (it->first.IsScalar()) {
      G4cout << "[Optics] water key: " << it->first.Scalar() << G4endl;
    }
  }
  auto rindex = load_double_vector(waterNode, {"rindex", "RINDEX"}, "water");
  auto absorptionMm = load_double_vector(waterNode,
                                         {"absorption_length_mm", "abs_length_mm", "ABSLENGTH", "absorption"},
                                         "water");
  auto scatteringMm = load_double_vector(waterNode,
                                         {"rayleigh_length_mm", "scattering_length_mm", "RAYLEIGH"},
                                         "water");

  if (rindex.size() != wavelengths.size() ||
      absorptionMm.size() != wavelengths.size() ||
      scatteringMm.size() != wavelengths.size()) {
    throw std::runtime_error("Water spectra lengths do not match wavelength grid in optics YAML '" + path + "'.");
  }

  reorder_in_place(rindex, order);
  reorder_in_place(absorptionMm, order);
  reorder_in_place(scatteringMm, order);

  YAML::Node surfacesNode = find_child_ci(root, "surfaces");

  auto wallNode = surfacesNode ? find_child_ci(surfacesNode, "wall") : find_child_ci(root, "wall");
  if (!wallNode || !wallNode.IsMap()) {
    throw std::runtime_error("Optics YAML '" + path + "' missing 'wall' section.");
  }
  auto wallReflectivity = load_double_vector(wallNode, {"reflectivity", "REFLECTIVITY"}, "wall");
  if (wallReflectivity.size() != wavelengths.size()) {
    throw std::runtime_error("Wall reflectivity spectrum length mismatch in optics YAML '" + path + "'.");
  }
  reorder_in_place(wallReflectivity, order);

  YAML::Node photocathodeNode = surfacesNode ? find_child_ci(surfacesNode, {"photocathode", "pmt"})
                                             : find_child_ci(root, {"pmt", "photocathode"});
  if (!photocathodeNode || !photocathodeNode.IsMap()) {
    throw std::runtime_error("Optics YAML '" + path + "' missing 'photocathode' section.");
  }
  VecD pmtQE;
  VecD pmtQESourceLambda;
  struct QECandidate {
    const YAML::Node* node;
    const char* key;
    double scale;
  };
  std::vector<QECandidate> qeCandidates;
  auto addCandidate = [&](const YAML::Node& node, const char* key, double scale) {
    if (node && node.IsMap()) {
      qeCandidates.push_back({&node, key, scale});
    }
  };

  addCandidate(photocathodeNode, "QE_curve", 1.0);
  addCandidate(root, "QE_curve", 1.0);
  addCandidate(photocathodeNode, "QE", 1.0);
  addCandidate(root, "QE", 1.0);
  addCandidate(photocathodeNode, "qe", 1.0);
  addCandidate(root, "qe", 1.0);
  addCandidate(photocathodeNode, "quantum_efficiency", 1.0);
  addCandidate(root, "quantum_efficiency", 1.0);
  addCandidate(photocathodeNode, "QE_percent", 0.01);
  addCandidate(root, "QE_percent", 0.01);
  addCandidate(photocathodeNode, "qe_percent", 0.01);
  addCandidate(root, "qe_percent", 0.01);
  addCandidate(photocathodeNode, "efficiency", 1.0);
  addCandidate(root, "efficiency", 1.0);
  addCandidate(photocathodeNode, "EFFICIENCY", 1.0);

  for (const auto& cand : qeCandidates) {
    auto node = find_child_ci(*cand.node, {cand.key});
    if (node && node.IsSequence()) {
      pmtQE = load_double_vector(*cand.node, {cand.key}, cand.key);
      for (double& v : pmtQE) v *= cand.scale;
      auto waveNode = find_child_ci(*cand.node, {"wavelength_nm", "WAVELENGTH_NM"});
      if (waveNode && waveNode.IsSequence()) {
        pmtQESourceLambda = load_double_vector(*cand.node, {"wavelength_nm", "WAVELENGTH_NM"}, "photocathode wavelengths");
      }
      break;
    }
  }

  if (pmtQE.empty()) {
    throw std::runtime_error("Photocathode efficiency spectrum missing in optics YAML '" + path + "'.");
  }

  pmtQE = resample_to_grid(pmtQESourceLambda, pmtQE, wavelengths);
  if (pmtQE.size() != wavelengths.size()) {
    pmtQE.assign(wavelengths.size(), pmtQE.empty() ? 0.0 : pmtQE.back());
  }
  for (double& v : pmtQE) {
    v = clip_unit_interval(v);
  }

  if (std::isfinite(qeOverride)) {
    for (double& v : pmtQE) {
      v = clip_unit_interval(v * qeOverride);
    }
    const double peak = pmtQE.empty() ? 0.0 : *std::max_element(pmtQE.begin(), pmtQE.end());
    G4cout << "[PMT.QE] override applied: scale=" << qeOverride
           << " new_peak=" << peak << G4endl;
  }

  double maxQEFraction = pmtQE.empty() ? 0.0 : *std::max_element(pmtQE.begin(), pmtQE.end());
  if (maxQEFraction <= 0.0 && !wavelengths.empty()) {
    VecD fallback(wavelengths.size(), 0.0);
    for (size_t i = 0; i < wavelengths.size(); ++i) {
      if (wavelengths[i] >= 300.0 && wavelengths[i] <= 500.0) {
        fallback[i] = 0.25;
      }
    }
    pmtQE.swap(fallback);
    maxQEFraction = 0.25;
    G4cout << "[PMT.QE] WARNING: loaded QE is zero; using fallback box QE 25% (300–500 nm)." << G4endl;
  }
  LogQESamples(wavelengths, pmtQE);

  VecD pmtReflectivity;
  if (auto refNode = find_child_ci(photocathodeNode, {"reflectivity", "REFLECTIVITY"})) {
    pmtReflectivity = load_double_vector(photocathodeNode, {"reflectivity", "REFLECTIVITY"}, "photocathode");
    if (pmtReflectivity.size() != wavelengths.size()) {
      throw std::runtime_error("Photocathode reflectivity spectrum length mismatch in optics YAML '" + path + "'.");
    }
    reorder_in_place(pmtReflectivity, order);
  } else {
    pmtReflectivity.assign(wavelengths.size(), 0.0);
  }

  auto sanitise_fraction = [](VecD& vec) {
    for (double& v : vec) {
      if (v > 1.0) v *= 0.01;
      v = clip_unit_interval(v);
    }
  };
  sanitise_fraction(wallReflectivity);
  sanitise_fraction(pmtReflectivity);

  OpticalPropertiesResult result;
  result.waterSummary = compute_water_summary(wavelengths, rindex, absorptionMm, scatteringMm);
  result.pmtSummary   = compute_pmt_summary(wavelengths, pmtQE);

  const double hc = h_Planck * c_light;
  const size_t npts = wavelengths.size();
  std::vector<G4double> energy;
  energy.reserve(npts);

  std::vector<G4double> rindexG4;      rindexG4.reserve(npts);
  std::vector<G4double> absorptionG4;  absorptionG4.reserve(npts);
  std::vector<G4double> scatteringG4;  scatteringG4.reserve(npts);
  std::vector<G4double> wallRefG4;     wallRefG4.reserve(npts);
  std::vector<G4double> pmtQEG4;       pmtQEG4.reserve(npts);
  std::vector<G4double> pmtRefG4;      pmtRefG4.reserve(npts);

  for (size_t idx = npts; idx-- > 0;) {
    const double lambda_nm = wavelengths[idx];
    const double energyVal = hc / (lambda_nm * nm);
    energy.push_back(energyVal);
    rindexG4.push_back(rindex[idx]);
    absorptionG4.push_back(absorptionMm[idx] * mm);
    scatteringG4.push_back(scatteringMm[idx] * mm);
    wallRefG4.push_back(wallReflectivity[idx]);
    pmtQEG4.push_back(pmtQE[idx]);
    pmtRefG4.push_back(pmtReflectivity[idx]);
  }

  auto* waterMPT = new G4MaterialPropertiesTable();
  waterMPT->AddProperty("RINDEX",     energy.data(), rindexG4.data(), energy.size());
  waterMPT->AddProperty("ABSLENGTH",  energy.data(), absorptionG4.data(), energy.size());
  waterMPT->AddProperty("RAYLEIGH",   energy.data(), scatteringG4.data(), energy.size());
  result.waterMPT = waterMPT;
  result.energyGrid = energy;
  result.wavelength_nm = wavelengths;

  const std::string wallName = get_string_ci(wallNode, {"name"}, "InnerWallSurface");
  auto* wallSurface = new G4OpticalSurface(wallName.c_str());
  wallSurface->SetModel(parse_model(get_string_ci(wallNode, {"model"}, "unified")));
  wallSurface->SetType(parse_type(get_string_ci(wallNode, {"type"}, "dielectric_dielectric")));
  wallSurface->SetFinish(parse_finish(get_string_ci(wallNode, {"finish"}, "ground")));
  bool sigmaFound = false;
  double sigmaAlpha = get_double_ci(wallNode, {"sigma_alpha"}, 0.0, &sigmaFound);
  if (sigmaFound) {
    wallSurface->SetSigmaAlpha(sigmaAlpha);
  }
  auto* wallMPT = new G4MaterialPropertiesTable();
  wallMPT->AddProperty("REFLECTIVITY", energy.data(), wallRefG4.data(), energy.size());
  wallSurface->SetMaterialPropertiesTable(wallMPT);
  result.wallSurface = wallSurface;

  const std::string pmtName = get_string_ci(photocathodeNode, {"name"}, "PhotocathodeSurface");
  auto* pmtSurface = new G4OpticalSurface(pmtName.c_str());
  pmtSurface->SetModel(parse_model(get_string_ci(photocathodeNode, {"model"}, "unified")));
  auto typeString = get_string_ci(photocathodeNode, {"type"}, "dielectric_dielectric");
  auto requestedType = parse_type(typeString);
  if (requestedType != dielectric_dielectric) {
    G4cout << "[Optics] Forcing photocathode surface type to dielectric_dielectric (config requested '"
           << typeString << "')\n";
  }
  pmtSurface->SetType(dielectric_dielectric);
  pmtSurface->SetFinish(parse_finish(get_string_ci(photocathodeNode, {"finish"}, "polished")));
  sigmaFound = false;
  sigmaAlpha = get_double_ci(photocathodeNode, {"sigma_alpha"}, 0.0, &sigmaFound);
  if (sigmaFound) {
    pmtSurface->SetSigmaAlpha(sigmaAlpha);
  }
  auto* pmtMPT = new G4MaterialPropertiesTable();
  pmtMPT->AddProperty("EFFICIENCY",   energy.data(), pmtQEG4.data(), energy.size());
  pmtMPT->AddProperty("REFLECTIVITY", energy.data(), pmtRefG4.data(), energy.size());
  pmtSurface->SetMaterialPropertiesTable(pmtMPT);
  result.photocathodeSurface = pmtSurface;
  result.photocathodeMaterial = BuildPhotocathodeMaterial(result.wavelength_nm);

  return result;
}

OpticalPropertiesResult OpticalProperties::LoadFromYaml(const std::string& path) {
  return LoadFromYaml(path, std::numeric_limits<double>::quiet_NaN());
}

void OpticalProperties::AttachVacuumRindex(G4Material* vacuum,
                                           const std::vector<G4double>& energies) {
  if (!vacuum || energies.empty()) return;
  auto* mpt = vacuum->GetMaterialPropertiesTable();
  if (!mpt) mpt = new G4MaterialPropertiesTable();

  std::vector<G4double> energyCopy(energies.begin(), energies.end());
  std::vector<G4double> ones(energies.size(), 1.0);
  mpt->AddProperty("RINDEX", energyCopy.data(), ones.data(), energyCopy.size());
  vacuum->SetMaterialPropertiesTable(mpt);
}

G4Material* OpticalProperties::BuildPhotocathodeMaterial(const std::vector<double>& wavelengthsNm,
                                                         double rindex) {
  static G4Material* cached = nullptr;
  if (cached) return cached;

  if (wavelengthsNm.empty()) {
    throw std::runtime_error("Photocathode wavelength grid is empty; cannot assign RINDEX.");
  }

  auto energies = WavelengthsToPhotonEnergy(wavelengthsNm);
  if (energies.empty()) {
    throw std::runtime_error("Failed to convert photocathode wavelength grid to photon energies.");
  }

  auto* nist = G4NistManager::Instance();
  auto* si = nist->FindOrBuildElement("Si");
  auto* mat = new G4Material("PhotocathodeMat", 2.0 * g / cm3, 1);
  mat->AddElement(si, 1);

  auto* mpt = new G4MaterialPropertiesTable();
  std::vector<G4double> rindexVec(energies.size(), rindex);
  mpt->AddProperty("RINDEX", energies.data(), rindexVec.data(), energies.size());
  mat->SetMaterialPropertiesTable(mpt);
  cached = mat;
  return mat;
}

void OpticalProperties::DumpWaterMPT(const G4Material* material,
                                     const G4String& waterVolumeName) {
  G4cout << "[Optics] Water volume='" << waterVolumeName << "'"
         << " material=" << (material ? material->GetName() : "<null>") << G4endl;
  if (!material) {
    return;
  }

  auto* mpt = material->GetMaterialPropertiesTable();
  if (!mpt) {
    G4cout << "[Optics] Water material has no material properties table." << G4endl;
    return;
  }

  auto* rindex   = mpt->GetProperty("RINDEX");
  auto* abslen   = mpt->GetProperty("ABSLENGTH");
  auto* rayleigh = mpt->GetProperty("RAYLEIGH");
  auto* mie      = mpt->GetProperty("MIEHG");

  auto vecLength = [](G4MaterialPropertyVector* vec) -> G4int {
    return vec ? static_cast<G4int>(vec->GetVectorLength()) : 0;
  };
  G4cout << "[Optics] Water MPT entries: "
         << "RINDEX=" << vecLength(rindex)
         << " ABSLENGTH=" << vecLength(abslen)
         << " RAYLEIGH=" << vecLength(rayleigh)
         << " MIE=" << vecLength(mie) << G4endl;

  auto sample = [](G4MaterialPropertyVector* vec, G4double energy) -> G4double {
    return vec ? vec->Value(energy) : 0.0;
  };

  const G4double e400 = (h_Planck * c_light) / (400.0 * nm);
  const G4double e450 = (h_Planck * c_light) / (450.0 * nm);
  G4cout << "[Optics] Water RINDEX samples: 400nm -> "
         << sample(rindex, e400)
         << " 450nm -> " << sample(rindex, e450) << G4endl;
}
