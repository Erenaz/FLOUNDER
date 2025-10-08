#include "OpticalPropertiesLoader.hh"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "G4Material.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"

using Row = struct { G4double lambda_nm, n, labs_m, lsca_m; };

static inline bool is_comment_or_blank(const std::string& s) {
  for (char c : s) { if (c == '#') return true; if (!std::isspace((unsigned char)c)) return false; }
  return true; // blank → treat as comment/skip
}

static std::vector<std::string> split_commas(const std::string& line) {
  std::vector<std::string> out; std::stringstream ss(line); std::string tok;
  while (std::getline(ss, tok, ',')) { // also trim spaces
    size_t a=0, b=tok.size();
    while (a<b && std::isspace((unsigned char)tok[a])) ++a;
    while (b>a && std::isspace((unsigned char)tok[b-1])) --b;
    out.emplace_back(tok.substr(a, b-a));
  }
  return out;
}

G4MaterialPropertiesTable* OpticalPropertiesLoader::BuildWaterMPTFromCSV(
    const std::string& csv_path, WaterOpticsSummary& out) {

  std::ifstream in(csv_path);
  if (!in) throw std::runtime_error("Cannot open water optics CSV: " + csv_path);

  std::vector<Row> rows; rows.reserve(512);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || is_comment_or_blank(line)) continue;
    auto cols = split_commas(line);
    if (cols.size() < 4) continue; // skip malformed
    Row r;
    r.lambda_nm = std::stod(cols[0]);
    r.n         = std::stod(cols[1]);
    r.labs_m    = std::stod(cols[2]);
    r.lsca_m    = std::stod(cols[3]);
    rows.push_back(r);
  }
  if (rows.size() < 2) throw std::runtime_error("Too few rows in water CSV: " + csv_path);

  // Convert to photon energy grid (ascending)
  std::vector<G4double> ENERGY, RINDEX, ABSLEN, RAYLEIGH;
  ENERGY.reserve(rows.size()); RINDEX.reserve(rows.size());
  ABSLEN.reserve(rows.size()); RAYLEIGH.reserve(rows.size());

  // Sort by increasing energy (i.e., decreasing wavelength)
  std::sort(rows.begin(), rows.end(),
            [](const Row& a, const Row& b){ return a.lambda_nm > b.lambda_nm; });

  out.lambda_min_nm = rows.back().lambda_nm;
  out.lambda_max_nm = rows.front().lambda_nm;

  G4double nmin=1e9, nmax=-1e9, amin=1e9, amax=-1e9, smin=1e9, smax=-1e9;

  for (const auto& r : rows) {
    const G4double E = (h_Planck * c_light) / (r.lambda_nm * nm); // Geant4 energy units
    ENERGY.push_back(E);
    RINDEX.push_back(r.n);
    ABSLEN.push_back(r.labs_m * m);
    RAYLEIGH.push_back(r.lsca_m * m);
    nmin = std::min(nmin, r.n); nmax = std::max(nmax, r.n);
    amin = std::min(amin, r.labs_m); amax = std::max(amax, r.labs_m);
    smin = std::min(smin, r.lsca_m); smax = std::max(smax, r.lsca_m);
  }

  auto* mpt = new G4MaterialPropertiesTable();
  mpt->AddProperty("RINDEX",    ENERGY, RINDEX);
  mpt->AddProperty("ABSLENGTH", ENERGY, ABSLEN);
  mpt->AddProperty("RAYLEIGH",  ENERGY, RAYLEIGH);

  out.npoints     = ENERGY.size();
  out.n_min       = nmin; out.n_max       = nmax;
  out.labs_min_m  = amin; out.labs_max_m  = amax;
  out.lsca_min_m  = smin; out.lsca_max_m  = smax;
  out.energy_grid = ENERGY; // copy

  return mpt;
}

void OpticalPropertiesLoader::AttachVacuumRindex(
    G4Material* vacuum, const std::vector<G4double>& energies) {

  if (!vacuum) return;
  auto* mpt = vacuum->GetMaterialPropertiesTable();
  if (!mpt) mpt = new G4MaterialPropertiesTable();

  std::vector<G4double> ones(energies.size(), 1.0);
  mpt->AddProperty("RINDEX", energies, ones);
  vacuum->SetMaterialPropertiesTable(mpt);
}

PMTSummary OpticalPropertiesLoader::LoadPMTQE(
    const std::string& csv_path, G4double mean_from_nm, G4double mean_to_nm) {

  std::ifstream in(csv_path);
  if (!in) throw std::runtime_error("Cannot open PMT QE CSV: " + csv_path);

  struct QErow { G4double lambda_nm, qe; };
  std::vector<QErow> rows; rows.reserve(512);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || is_comment_or_blank(line)) continue;
    auto cols = split_commas(line);
    if (cols.size() < 2) continue;
    QErow r;
    r.lambda_nm = std::stod(cols[0]);
    double qeval = std::stod(cols[1]);
    if (qeval > 1.0) qeval *= 0.01; // accept percentages
    r.qe = qeval;
    rows.push_back(r);
  }
  if (rows.size() < 2) throw std::runtime_error("No/too few rows in PMT QE CSV: " + csv_path);

  // Sort by increasing wavelength (nm)
  std::sort(rows.begin(), rows.end(),
            [](const QErow& a, const QErow& b){ return a.lambda_nm < b.lambda_nm; });

  PMTSummary s; s.npoints = rows.size();
  s.lambda_min_nm = rows.front().lambda_nm;
  s.lambda_max_nm = rows.back().lambda_nm;

  std::vector<G4double> ENERGY; ENERGY.reserve(rows.size());
  std::vector<G4double> EFF;    EFF.reserve(rows.size());

  // --- Weighted mean over [mean_from_nm, mean_to_nm] using trapezoids with segment clipping ---
  const double A = std::max<double>(mean_from_nm, s.lambda_min_nm);
  const double B = std::min<double>(mean_to_nm,   s.lambda_max_nm);
  double area = 0.0, width = 0.0;

  auto q_at = [&](size_t i, double x)->double {
    // linear interpolate within segment [i,i+1]
    double x0 = rows[i].lambda_nm, x1 = rows[i+1].lambda_nm;
    double y0 = rows[i].qe,        y1 = rows[i+1].qe;
    if (x1 == x0) return 0.5*(y0+y1);
    double t = (x - x0) / (x1 - x0);
    return y0 + t*(y1 - y0);
  };

  const double hc = h_Planck * c_light;
  for (auto it = rows.rbegin(); it != rows.rend(); ++it) {
    const G4double E = hc / (it->lambda_nm * nm);
    ENERGY.push_back(E);
    EFF.push_back(it->qe);
  }

  if (A < B) {
    for (size_t i = 0; i+1 < rows.size(); ++i) {
      double x0 = rows[i].lambda_nm, x1 = rows[i+1].lambda_nm;
      // overlap with [A,B]
      double L = std::max(x0, A);
      double R = std::min(x1, B);
      if (L >= R) continue;
      // endpoints (interpolate if clipping)
      double qL = (L == x0 ? rows[i].qe     : q_at(i, L));
      double qR = (R == x1 ? rows[i+1].qe   : q_at(i, R));
      area  += 0.5*(qL + qR) * (R - L);
      width += (R - L);
    }
  }
  s.mean_qe_400_450 = (width > 0 ? area / width : 0.0);
  s.energy     = ENERGY;
  s.efficiency = EFF;
  return s;
}
