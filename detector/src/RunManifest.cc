#include "RunManifest.hh"

#include <TFile.h>
#include <TNamed.h>

#include <algorithm>
#include <cmath>
#include <vector>
#include <iomanip>
#include <ios>
#include <sstream>
#include <utility>

namespace {

RunManifest gManifest;
bool gManifestSet = false;
std::vector<TFile*> gRegisteredFiles;

std::string JsonEscape(const std::string& in) {
  std::ostringstream os;
  for (char c : in) {
    switch (c) {
      case '\\': os << "\\\\"; break;
      case '"':  os << "\\\""; break;
      case '\n': os << "\\n"; break;
      case '\r': os << "\\r"; break;
      case '\t': os << "\\t"; break;
      default:
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 0x20) {
          os << "\\u"
             << std::hex << std::uppercase
             << std::setfill('0') << std::setw(4)
             << static_cast<int>(uc)
             << std::nouppercase << std::dec
             << std::setfill(' ');
        } else {
          os << c;
        }
        break;
    }
  }
  return os.str();
}

std::string BuildManifestJson(const RunManifest& m) {
  std::ostringstream os;
  os << "{";
  auto appendKV = [&](const std::string& key, const std::string& value, bool last = false) {
    os << "\"" << key << "\":\"" << JsonEscape(value) << "\"";
    if (!last) os << ",";
  };
  auto appendBool = [&](const std::string& key, bool value, bool last = false) {
    os << "\"" << key << "\":" << (value ? "true" : "false");
    if (!last) os << ",";
  };

  appendKV("profile", m.profile);
  appendKV("macro", m.macro);
  appendKV("optics_path", m.opticsPath);
  appendKV("optics_contents", m.opticsContents);
  appendKV("pmt_path", m.pmtPath);
  appendKV("pmt_contents", m.pmtContents);
  appendKV("git_sha", m.gitSHA);
  appendKV("build_type", m.buildType);
  appendKV("compiler", m.compiler);
  appendKV("cxx_flags", m.cxxFlags);
  appendBool("digitizer_enabled", m.digitizerEnabled);
  appendKV("digitizer_output", m.digitizerOutput);
  appendKV("optics_override", m.opticsOverride);
  appendBool("quiet", m.quiet);
  appendKV("optical_verbose", std::to_string(m.opticalVerboseLevel));
  appendKV("qe_scale_override", std::isfinite(m.qeScaleOverride) ? std::to_string(m.qeScaleOverride) : "nan");
  appendKV("qe_flat_override", std::isfinite(m.qeFlatOverride) ? std::to_string(m.qeFlatOverride) : "nan", true);
  os << "}";
  return os.str();
}

} // namespace

void SetRunManifest(RunManifest manifest) {
  gManifest = std::move(manifest);
  gManifestSet = true;
}

const RunManifest& GetRunManifest() {
  return gManifest;
}

void RegisterOutputFile(TFile* file) {
  if (!file) return;
  if (std::find(gRegisteredFiles.begin(), gRegisteredFiles.end(), file) == gRegisteredFiles.end()) {
    gRegisteredFiles.push_back(file);
  }
}

void WriteManifestToFile(TFile* file, const std::string& objectName) {
  if (!file || !gManifestSet) return;
  const std::string json = BuildManifestJson(gManifest);
  file->cd();
  auto* named = dynamic_cast<TNamed*>(file->Get(objectName.c_str()));
  if (named) {
    named->SetTitle(json.c_str());
    named->Write(objectName.c_str(), TObject::kOverwrite);
  } else {
    TNamed manifest(objectName.c_str(), json.c_str());
    manifest.Write();
  }
}

void FlushManifestToOutputs() {
  for (auto* file : gRegisteredFiles) {
    WriteManifestToFile(file);
  }
}
