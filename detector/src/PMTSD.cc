#include "PMTSD.hh"

#include "G4MaterialPropertiesTable.hh"
#include "G4OpticalPhoton.hh"
#include "G4PhysicalConstants.hh"
#include "G4RunManager.hh"
#include "G4SDManager.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4Event.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4PhysicalVolumeStore.hh"

#include <algorithm>
#include <atomic>
#include <sstream>
#include <vector>

PMTSD::PMTSD(const G4String& name) : G4VSensitiveDetector(name) {
  collectionName.push_back("OpticalHits");
}

void PMTSD::Initialize(G4HCofThisEvent* hce) {
  hits_ = new PMTHitsCollection(SensitiveDetectorName, collectionName[0]);
  if (hc_id_ < 0) hc_id_ = G4SDManager::GetSDMpointer()->GetCollectionID(hits_);
  hce->AddHitsCollection(hc_id_, hits_);
  hitsThisEvent_ = 0;
  auto* rm = G4RunManager::GetRunManager();
  currentEventId_ = (rm && rm->GetCurrentEvent()) ? rm->GetCurrentEvent()->GetEventID() : -1;
  LogAttachmentsOnce();
}

G4bool PMTSD::ProcessHits(G4Step* step, G4TouchableHistory*) {
  if (auto* trk = step->GetTrack(); trk && trk->GetDefinition() == G4OpticalPhoton::OpticalPhotonDefinition()) {
    static std::atomic<int> seen{0};
    if (seen.fetch_add(1) < 10) {
      const auto* post = step->GetPostStepPoint();
      auto* postPV = post ? post->GetPhysicalVolume() : nullptr;
      G4cout << "[PMTSD:PhotonStep] pos=" << (post ? post->GetPosition() : G4ThreeVector())
             << " volume=" << (postPV ? postPV->GetName() : "<null>")
             << G4endl;
    }
  }

  auto* track = step->GetTrack();
  if (track->GetDefinition() != G4OpticalPhoton::OpticalPhotonDefinition()) {
    return false;
  }

  const auto* post = step->GetPostStepPoint();
  const auto* pre  = step->GetPreStepPoint();
  if (!post || !pre) return false;

  auto postTouchable = post->GetTouchableHandle();
  auto preTouchable  = pre->GetTouchableHandle();
  if (!postTouchable || !preTouchable) return false;
  auto* postPV = postTouchable->GetVolume();
  auto* prePV  = preTouchable->GetVolume();
  if (!postPV || !prePV) return false;

  auto* postLV = postPV->GetLogicalVolume();
  if (!postLV) return false;
  if (postLV->GetName() != "PMT_cathode_log") return false;
  if (prePV->GetLogicalVolume() == postLV) return false;

  const int copy = postPV->GetCopyNo();
  const G4double time = post->GetGlobalTime();
  const G4double energy = pre->GetKineticEnergy();
  G4double wavelength_nm = 0.0;
  if (energy > 0.0) {
    wavelength_nm = (h_Planck * c_light / energy) / nm;
  }

  hits_->insert(new PMTHit(copy, time, 0.0, wavelength_nm, /*flags=*/0));
  ++totalHits_;
  ++hitsThisEvent_;

  track->SetTrackStatus(fStopAndKill);
  return true;
}

void PMTSD::EndOfEvent(G4HCofThisEvent*) {
  auto* runManager = G4RunManager::GetRunManager();
  if (!runManager) return;
  auto* event = runManager->GetCurrentEvent();
  if (!event) return;

  const auto totalEvents = runManager->GetNumberOfEventsToBeProcessed();
  if (totalEvents <= 0) return;

  if (currentEventId_ >= 0) {
    G4cout << "[OPT_DBG] event=" << currentEventId_
           << " n_optical_hits=" << hitsThisEvent_ << G4endl;
  } else {
    G4cout << "[OPT_DBG] event=<unknown> n_optical_hits=" << hitsThisEvent_ << G4endl;
  }
  hitsThisEvent_ = 0;

  if (event->GetEventID() + 1 == totalEvents) {
    G4cout << "[HITS] n_pmt_hits=" << totalHits_ << G4endl;
    totalHits_ = 0;
  }
}

void PMTSD::LogAttachmentsOnce() {
  if (attachmentsLogged_) return;
  attachmentsLogged_ = true;

  auto* lvStore = G4LogicalVolumeStore::GetInstance();
  auto* pvStore = G4PhysicalVolumeStore::GetInstance();
  if (!lvStore) return;

  for (auto* lv : *lvStore) {
    if (!lv) continue;
    if (lv->GetSensitiveDetector() != this) continue;

    std::vector<int> copies;
    if (pvStore) {
      for (auto* pv : *pvStore) {
        if (pv && pv->GetLogicalVolume() == lv) {
          copies.push_back(pv->GetCopyNo());
        }
      }
    }
    std::sort(copies.begin(), copies.end());
    std::ostringstream oss;
    const std::size_t maxList = 16;
    for (std::size_t i = 0; i < copies.size() && i < maxList; ++i) {
      if (i) oss << ",";
      oss << copies[i];
    }
    if (copies.size() > maxList) {
      oss << ",...";
    }
    G4cout << "[PMTSD] attached_lv=" << lv->GetName()
           << " copies=" << copies.size()
           << " indices=" << oss.str() << G4endl;
  }
}
