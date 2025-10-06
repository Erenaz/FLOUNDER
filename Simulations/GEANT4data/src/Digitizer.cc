#include "Digitizer.hh"
#include <cmath>
#include <random>

static thread_local std::mt19937_64 rng{0xD1A9u};

double Digitizer::randu() const {
  return std::generate_canonical<double, 53>(rng);
}
double Digitizer::randn() const {
  // Box–Muller
  double u1 = std::max(1e-12, randu());
  double u2 = std::max(1e-12, randu());
  return std::sqrt(-2.0*std::log(u1))*std::cos(2*M_PI*u2);
}

void Digitizer::Digitize(int event, const std::vector<HitCandidate>& cand,
                         std::vector<DigiHit>& out) const {
  out.reserve(out.size() + cand.size());
  for (const auto& h : cand) {
    // QE sampling (flat; extend to QE(λ) later if you store lambda_nm)
    if (randu() > P.qe_flat) continue;

    // TTS + electronics jitter
    double t = h.t_ns + P.tts_sigma_ns*randn() + P.elec_jitter_ns*randn();

    // threshold (per-hit PE is ~1; if you later cluster in a window, apply thr there)
    if (1.0 >= P.thr_pe) {
      out.push_back(DigiHit{event, (int16_t)h.pmt_id, (float)t, 1.0f});
    }
  }
}

void Digitizer::AddDarkNoise(int event, double t0_ns, std::vector<DigiHit>& out) const {
  const double mean_per_pmt = P.dark_rate_hz * (P.window_ns*1e-9);
  // Poisson via Knuth for modest means
  for (unsigned ch = 0; ch < P.n_pmt; ++ch) {
    // draw k ~ Poisson(mean_per_pmt)
    double L = std::exp(-mean_per_pmt);
    int k = 0; double p = 1.0;
    do { ++k; p *= randu(); } while (p > L);
    int n = k-1;
    for (int i=0;i<n;++i) {
      double u = randu();                 // uniform in window
      double t = t0_ns + u*P.window_ns + P.elec_jitter_ns*randn();
      out.push_back(DigiHit{event, (int16_t)ch, (float)t, 1.0f});
    }
  }
}
