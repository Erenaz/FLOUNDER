void make_flounder_geo() {
  // Load ROOT geometry system
  TGeoManager *geom = new TGeoManager("flounder", "3m x 40m cylindrical water detector");

  // Define materials and media
  TGeoMaterial *matVacuum = new TGeoMaterial("Vacuum", 0, 0, 0);
  TGeoMedium   *vacuum = new TGeoMedium("Vacuum", 1, matVacuum);

  TGeoMaterial *matWater = new TGeoMaterial("Water", 18.01528, 10, 1.0); // simplified
  TGeoMedium   *water = new TGeoMedium("Water", 2, matWater);

  // Top world volume: make it a box slightly larger than the cylinder
  TGeoVolume *top = geom->MakeBox("TOP", vacuum, 3.0, 3.0, 21.0); // Half-lengths
  geom->SetTopVolume(top);

  // Water cylinder (R = 1.5 m, H = 40 m)
  TGeoVolume *detector = geom->MakeTube("FLOUNDER", water, 0.0, 1.5, 20.0); // Half-length = 20 m
  top->AddNode(detector, 1, new TGeoTranslation(0, 0, 0));

  geom->CloseGeometry();
  geom->Export("flounder_geo.root");
}

