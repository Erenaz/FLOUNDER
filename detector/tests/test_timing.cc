#include "Digitizer.hh"

#include "G4ThreeVector.hh"
#include "Randomize.hh"

#include <CLHEP/Random/Randomize.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <vector>

namespace PrimaryInfo {
static G4ThreeVector gX0(0., 0., 0.);
static double gT0 = 0.0;
void Set(const G4ThreeVector& x0, double t0_ns) {
  gX0 = x0;
  gT0 = t0_ns;
}
const G4ThreeVector& X0() { return gX0; }
double T0ns() { return gT0; }
} // namespace PrimaryInfo

namespace {

double compute_sigma(const std::vector<double>& values) {
  if (values.empty()) return 0.0;
  const double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
  double accum = 0.0;
  for (double v : values) {
    const double d = v - mean;
    accum += d * d;
  }
  return std::sqrt(accum / values.size());
}

std::vector<HitCandidate> make_hits(std::size_t n, int pmt_id, double t_ns, double lambda_nm) {
  std::vector<HitCandidate> hits;
  hits.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    hits.push_back({pmt_id, t_ns, lambda_nm});
  }
  return hits;
}

std::vector<double> extract_times(const std::vector<DigiHit>& hits) {
  std::vector<double> out;
  out.reserve(hits.size());
  for (const auto& h : hits) {
    out.push_back(h.t_ns);
  }
  return out;
}

} // namespace

int main() {
  PrimaryInfo::Set(G4ThreeVector(), 0.0);

  DigitizerParams base;
  base.QE = 1.0;
  base.TTS_ns = 0.0;
  base.JITTER_ns = 0.0;
  base.DARK_HZ = 0.0;
  base.THRESH_PE = 0.0;
  base.TWIN_LO_ns = -1e6;
  base.TWIN_HI_ns = 1e6;

  const auto hits = make_hits(4000, 42, 100.0, 400.0);

  CLHEP::HepRandom::setTheSeed(12345);
  Digitizer digiNoSmear(base);
  std::vector<DigiHit> outNoSmear;
  digiNoSmear.Digitize(0, hits, outNoSmear);
  const double sigmaNoSmear = compute_sigma(extract_times(outNoSmear));

  std::cout << "[timing] sigma(TTS=0,J=0) = " << sigmaNoSmear << " ns" << std::endl;
  if (sigmaNoSmear >= 1e-6) {
    std::cerr << "[timing] FAIL: zero-smear case has sigma >= 1 ps.\n";
    return 1;
  }

  DigitizerParams smear = base;
  smear.TTS_ns = 0.9;
  smear.JITTER_ns = 0.4;

  CLHEP::HepRandom::setTheSeed(67890);
  Digitizer digiSmear(smear);
  std::vector<DigiHit> outSmear;
  digiSmear.Digitize(1, hits, outSmear);
  const double sigmaSmear = compute_sigma(extract_times(outSmear));
  const double expectedSigma = std::sqrt(smear.TTS_ns * smear.TTS_ns +
                                         smear.JITTER_ns * smear.JITTER_ns);

  std::cout << "[timing] sigma(TTS=0.9ns,J=0.4ns) = " << sigmaSmear
            << " ns (expected ~" << expectedSigma << ")\n";

  if (sigmaSmear < 0.5 * expectedSigma || sigmaSmear > 1.5 * expectedSigma) {
    std::cerr << "[timing] FAIL: smear sigma outside tolerance.\n";
    return 1;
  }

  std::cout << "[timing] PASS\n";
  return 0;
}
