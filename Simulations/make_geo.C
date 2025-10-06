#include <TGeoManager.h>

void make_geo(const char* outfile = "fln_geo.gdml") {
  // Initialize geometry manager
  TGeoManager* geoMan = new TGeoManager("FLOUNDER", "Water Cylinder Detector");

  // Define materials
  TGeoMaterial* matVac   = new TGeoMaterial("Vacuum", 0, 0, 0);
  TGeoMaterial* matWater = new TGeoMaterial("Water", 18.015, 10., 1.0); // A, Z, rho

  TGeoMedium* vac   = new TGeoMedium("Vac", 1, matVac);
  TGeoMedium* water = new TGeoMedium("Water", 2, matWater);

  // World volume: a large box of vacuum (6×6×80 m³)
  TGeoVolume* world = geoMan->MakeBox("TopVolume", vac, 300, 300, 4000); // in cm
  geoMan->SetTopVolume(world);
  world->SetLineColor(kGray + 2); // Dark gray

  // Detector volume: water-filled cylinder (3 m diameter, 40 m length)
  double Rdet = 150;   // cm
  double Ldet = 2000;  // cm (half-length)
  TGeoVolume* det = geoMan->MakeTube("Detector", water, 0., Rdet, Ldet);
  det->SetLineColor(kAzure + 1); // Blue

  // Place detector at the center of the world
  world->AddNode(det, 1, new TGeoTranslation(0, 0, 0));

  // Finalize geometry
  geoMan->CloseGeometry();

  // Export GDML
  geoMan->Export(outfile, "GDML");

  printf("Geometry exported to %s\n", outfile);
}
