#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct DigitizerParams {
  double qe_flat = 0.25;          // flat QE if no λ info
  double tts_sigma_ns = 1.3;      // PMT transit-time spread (ns RMS)
  double elec_jitter_ns = 0.2;    // electronics jitter (ns RMS)
  double dark_rate_hz = 5000.0;   // per-PMT dark noise rate
  double window_ns = 300.0;       // readout window length
  double thr_pe = 0.5;            // per-PMT discriminator threshold (PE)
  unsigned n_pmt = 1000;          // logical channel count (adjust to your geom)
};

struct HitCandidate {             // from stepping at PMT boundary
  int pmt_id;                     // channel index
  double t_ns;                    // arrival time (global)
  double lambda_nm;               // optional; 0 if unknown
};

struct DigiHit {
  int event;
  int16_t pmt;
  float t_ns;                     // digitized time (ns)
  float npe;                      // typically 1.0 after threshold
};

class Digitizer {
public:
  explicit Digitizer(const DigitizerParams& p): P(p) {}
  void Digitize(int event, const std::vector<HitCandidate>& cand,
                std::vector<DigiHit>& out) const;

  // add dark noise for each PMT over [t0, t0+window]
  void AddDarkNoise(int event, double t0_ns, std::vector<DigiHit>& out) const;

private:
  DigitizerParams P;
  double randn() const;           // Box–Muller
  double randu() const;           // [0,1)
};
