#include "PMTSD.hh"

#include "G4OpticalPhoton.hh"
#include "G4OpBoundaryProcess.hh"
#include "G4ProcessManager.hh"
#include "G4RunManager.hh"
#include "G4SDManager.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4Event.hh"

PMTSD::PMTSD(const G4String& name) : G4VSensitiveDetector(name) {
  collectionName.insert("PMTHits");
}

void PMTSD::Initialize(G4HCofThisEvent* hce) {
  hits_ = new PMTHitsCollection(SensitiveDetectorName, collectionName[0]);
  if (hc_id_ < 0) hc_id_ = G4SDManager::GetSDMpointer()->GetCollectionID(hits_);
  hce->AddHitsCollection(hc_id_, hits_);
}

G4bool PMTSD::ProcessHits(G4Step* step, G4TouchableHistory*) {
  if (step->GetTrack()->GetDefinition() != G4OpticalPhoton::OpticalPhotonDefinition()) {
    return false;
  }

  if (step->GetPostStepPoint()->GetStepStatus() != fGeomBoundary) {
    return false;
  }

  static G4OpBoundaryProcess* boundary = nullptr;
  if (!boundary) {
    auto* procMgr = step->GetTrack()->GetDefinition()->GetProcessManager();
    if (procMgr) {
      auto* procList = procMgr->GetProcessList();
      for (G4int i = 0; i < procMgr->GetProcessListLength(); ++i) {
        auto* proc = (*procList)[i];
        if (auto* b = dynamic_cast<G4OpBoundaryProcess*>(proc)) {
          boundary = b;
          break;
        }
      }
    }
  }
  if (!boundary) return false;

  if (boundary->GetStatus() != Detection) {
    return false;
  }

  const auto* post = step->GetPostStepPoint();
  if (!post) return false;
  auto touchable = post->GetTouchable();
  if (!touchable) return false;
  auto* postVolume = touchable->GetVolume();
  if (!postVolume || postVolume->GetName() != "PMT") return false;

  int copy = postVolume->GetCopyNo();
  double t = step->GetPreStepPoint()->GetGlobalTime();
  hits_->insert(new PMTHit(copy, t, 1.0));

  ++totalHits_;
  step->GetTrack()->SetTrackStatus(fStopAndKill);
  return true;
}

void PMTSD::EndOfEvent(G4HCofThisEvent*) {
  auto* runManager = G4RunManager::GetRunManager();
  if (!runManager) return;
  auto* event = runManager->GetCurrentEvent();
  if (!event) return;

  const auto totalEvents = runManager->GetNumberOfEventsToBeProcessed();
  if (totalEvents <= 0) return;

  if (event->GetEventID() + 1 == totalEvents) {
    G4cout << "[HITS] n_pmt_hits=" << totalHits_ << G4endl;
    totalHits_ = 0;
  }
}
