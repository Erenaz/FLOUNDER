#pragma once

#include <string>
#include <limits>

class TFile;

#ifndef FLNDR_GIT_SHA
#define FLNDR_GIT_SHA "unknown"
#endif
#ifndef FLNDR_BUILD_TYPE
#define FLNDR_BUILD_TYPE "unknown"
#endif
#ifndef FLNDR_COMPILER
#define FLNDR_COMPILER "unknown"
#endif
#ifndef FLNDR_CXX_FLAGS
#define FLNDR_CXX_FLAGS ""
#endif

struct RunManifest {
  std::string profile;
  std::string macro;
  std::string opticsPath;
  std::string opticsContents;
  std::string pmtPath;
  std::string pmtContents;
  std::string gitSHA;
  std::string buildType;
  std::string compiler;
  std::string cxxFlags;
  bool digitizerEnabled = false;
  std::string digitizerOutput;
  std::string opticsOverride;
  bool opticalDebug = false;
  bool quiet = false;
  int  opticalVerboseLevel = 0;
  int  summaryEvery = 0;
  double qeScaleOverride = std::numeric_limits<double>::quiet_NaN();
  double qeFlatOverride = std::numeric_limits<double>::quiet_NaN();
  double thresholdPEOverride = std::numeric_limits<double>::quiet_NaN();
};

void SetRunManifest(RunManifest manifest);
const RunManifest& GetRunManifest();
void RegisterOutputFile(TFile* file);
void WriteManifestToFile(TFile* file, const std::string& objectName = "run_manifest");
void FlushManifestToOutputs();
