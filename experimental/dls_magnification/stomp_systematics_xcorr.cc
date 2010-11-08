#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>
#include <sstream>
#include <stomp.h>
#include <gflags/gflags.h>


// Define our command-line flags.
DEFINE_string(sys_file, "",
              "ASCII file containing uniform resolution sysmap");
DEFINE_string(sysmap_file, "",
              "ASCII file containing uniform resolution sysmap");
DEFINE_string(output_tag, "test",
              "Tag for output file: MeanWthetaSys_OUTPUT_TAG_combined");
DEFINE_double(seeing_min, 0.5, "Minimum allowable seeing value");
DEFINE_double(seeing_max, 2.0, "Maximum allowable seeing value");
DEFINE_double(extinction_min, 0.0, "Minimum allowable extinction value");
DEFINE_double(extinction_max, 1.0, "Maximum allowable extinction value");
DEFINE_double(sky_min, 0.0, "Minimum allowable sky brightness value");
DEFINE_double(sky_max, 24.0, "Maximum allowable sky brightness value");
DEFINE_string(sys_type, "sky", "Systematics type To Correlate");
DEFINE_bool(sys_seeing, false,
	    "Cross-correlate galaxy density and seeing variations.");
DEFINE_bool(sys_extinction, false,
	    "Cross-correlate galaxy density and extinction variations.");
DEFINE_bool(sys_sky, false,
	    "Cross-correlate galaxy density and sky brightness variations.");
DEFINE_int32(n_jack, -1,
	     "Number of jack-knife samples to use; default to 2*n_thetabins.");
DEFINE_double(theta_min, 0.09, "Minimum angular scale (in degrees)");
DEFINE_double(theta_max, 2.0, "Maximum angular scale (in degrees)");
DEFINE_double(galmap_resolution, 2048, "Resolution of Galaxy Map");
DEFINE_int32(n_bins_per_decade, 8, "Number of angular bins per decade.");

typedef std::map<const uint32_t, int> KeepDict;
typedef KeepDict::iterator KeepDictIterator;
//Stomp::ScalarVector sys_pix;

enum SystematicsField {
  Seeing,
  Extinction,
  Sky
};

int main(int argc, char **argv) {
  void FindAllowedArea(KeepDict& keep_map);
  Stomp::ScalarMap* LoadSysData(KeepDict& keep_map,
				int region_resolution,
				SystematicsField sys_field);
  void AutoCorrelateGalaxies(Stomp::ScalarMap* galaxy_map,
			     Stomp::AngularCorrelation& wtheta);
  void CrossCorrelateSystematicsField(Stomp::ScalarMap* galaxy_map,
				      SystematicsField sys_field,
				      Stomp::AngularCorrelation& wtheta);

  std::string usage = "Usage: ";
  usage += argv[0];
  usage += " --sys_file=<SysMap ASCII>";
  usage += " --sysmap_file=<SysMap ASCII>";
  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_sys_file.empty()) {
    std::cout << usage << "\n";
    std::cout << "Type '" << argv[0] << " --help' for a list of options.\n";
    exit(1);
  }

  if (FLAGS_sysmap_file.empty()) {
    std::cout << usage << "\n";
    std::cout << "Type '" << argv[0] << " --help' for a list of options.\n";
    exit(1);
  }

  std::cout << "Systematics bounds:\n";
  std::cout <<
    "\t" << FLAGS_seeing_min << " < seeing < " << FLAGS_seeing_max << "\n";
  std::cout <<
    "\t" << FLAGS_extinction_min << " < extinction < " <<
    FLAGS_extinction_max << "\n";
  std::cout <<
    "\t" << FLAGS_sky_min << " < sky brightness < " << FLAGS_sky_max << "\n";

  // Now we read in our sysmap.  We only want to keep those pixels that
  // meet our selection criteria, so we filter on those as we read through
  // the file.
  KeepDict keep_map;
  FindAllowedArea(keep_map);

  // We need some constraints from our angular binning, so we'll make a
  // single AngularCorrelation object at the outset.  If we do the galaxy
  // auto-correlation, then we'll use the object for that calculation.
  Stomp::AngularCorrelation wtheta(FLAGS_theta_min, FLAGS_theta_max,
				   FLAGS_n_bins_per_decade);

  // If there's no specifed number of jack-knife samples, then we default to
  // twice the number of angular bins.
  if (FLAGS_n_jack == -1) FLAGS_n_jack = 2*wtheta.NBins();

  // Now, read in the galaxy map.  We only want to keep those pixels that match
  // the ones that passed through our systematics cuts.
  
  Stomp::ScalarMap* sys_map;
  if (FLAGS_sys_type=="seeing") {
      sys_map = LoadSysData(keep_map, wtheta.MinResolution(), Seeing);
    }
  else if (FLAGS_sys_type=="sky") {
      sys_map = LoadSysData(keep_map, wtheta.MinResolution(), Sky);
    }
  else if (FLAGS_sys_type=="extinction") {
      sys_map = LoadSysData(keep_map, wtheta.MinResolution(), Extinction);
    }
  else {
    std::cout<<"Systematics Type Not Valid. Valid Types are"<<
      "\n\t'seeing'\n\t'sky'\n\t'extinction'\n";
    exit(1);
  }

  // Set up the file name suffix that specifies our systematics cuts.
  std::ostringstream output_suffix;
  output_suffix << "_see" << FLAGS_seeing_min << "-" << FLAGS_seeing_max;
  output_suffix << "_ext" << FLAGS_extinction_min << "-" <<
    FLAGS_extinction_max;
  output_suffix << "_sky" << FLAGS_sky_min << "-" << FLAGS_sky_max;
  output_suffix << "_combined";

  // First, handle the galaxy auto-correlation case

  // Sys-Seeing
  if (FLAGS_sys_seeing) {
    Stomp::AngularCorrelation xcorr_seeing(FLAGS_theta_min, FLAGS_theta_max,
					   FLAGS_n_bins_per_decade);

    CrossCorrelateSystematicsField(sys_map, Seeing,
				   xcorr_seeing);

    std::string output_file_name =
      "MeanWthetaSys_"+FLAGS_sys_type+"-see_" + FLAGS_output_tag + output_suffix.str();
    std::cout << "Writing results to " << output_file_name << "...\n";

    std::ofstream output_file(output_file_name.c_str());
    for (Stomp::ThetaIterator iter=xcorr_seeing.Begin();
	 iter!=xcorr_seeing.End();++iter) {
      if (iter->Resolution() != -1) {
	output_file << std::setprecision(6) << iter->Theta() << " " <<
	  iter->MeanWtheta()  << " " << iter->MeanWthetaError() << "\n";
      }
    }
    output_file.close();
  }

  // Sys-Extinction
  if (FLAGS_sys_extinction) {
    Stomp::AngularCorrelation xcorr_extinction(FLAGS_theta_min, FLAGS_theta_max,
					       FLAGS_n_bins_per_decade);

    CrossCorrelateSystematicsField(sys_map, Extinction,
				   xcorr_extinction);

    std::string output_file_name =
      "MeanWthetaSys_"+FLAGS_sys_type+"-ext_" + FLAGS_output_tag + output_suffix.str();
    std::cout << "Writing results to " << output_file_name << "...\n";

    std::ofstream output_file(output_file_name.c_str());
    for (Stomp::ThetaIterator iter=xcorr_extinction.Begin();
	 iter!=xcorr_extinction.End();++iter) {
      if (iter->Resolution() != -1) {
	output_file << std::setprecision(6) << iter->Theta() << " " <<
	  iter->MeanWtheta()  << " " << iter->MeanWthetaError() << "\n";
      }
    }
    output_file.close();
  }

  // Sys-Sky Brightness
  if (FLAGS_sys_sky) {
    Stomp::AngularCorrelation xcorr_sky(FLAGS_theta_min, FLAGS_theta_max,
					   FLAGS_n_bins_per_decade);

    CrossCorrelateSystematicsField(sys_map, Sky,
				   xcorr_sky);

    std::string output_file_name =
      "MeanWthetaSys_"+FLAGS_sys_type+"-sky_" + FLAGS_output_tag + output_suffix.str();
    std::cout << "Writing results to " << output_file_name << "...\n";

    std::ofstream output_file(output_file_name.c_str());
    for (Stomp::ThetaIterator iter=xcorr_sky.Begin();
	 iter!=xcorr_sky.End();++iter) {
      if (iter->Resolution() != -1) {
	output_file << std::setprecision(6) << iter->Theta() << " " <<
	  iter->MeanWtheta()  << " " << iter->MeanWthetaError() << "\n";
      }
    }
    output_file.close();
  }

  std::cout << "Done.\n";

  return 0;
}

void FindAllowedArea(KeepDict& keep_map) {
  std::ifstream sysmap_file(FLAGS_sysmap_file.c_str());
  double unmasked, seeing, extinction, sky;
  uint32_t x, y, hpixnum, superpixnum, resolution, n_objects;
  double total_area = 0.0;
  

  std::cout << "Reading systematics map from " << FLAGS_sysmap_file << "...\n";
  unsigned long n_pixel = 0, n_keep = 0;
  unsigned long n_seeing = 0, n_extinction = 0, n_sky = 0, n_blank = 0;
  while (!sysmap_file.eof()) {
    sysmap_file >> hpixnum >> superpixnum >> resolution >> unmasked >> seeing >>
      extinction >> sky >> n_objects;

    if (!sysmap_file.eof()) {
      n_pixel++;
      if ((seeing >= FLAGS_seeing_min) &&
	  (seeing <= FLAGS_seeing_max) &&
	  (extinction >= FLAGS_extinction_min) &&
	  (extinction <= FLAGS_extinction_max) &&
	  (sky >= FLAGS_sky_min) &&
	  (sky <= FLAGS_sky_max) &&
	  (n_objects > 0)) {
	Stomp::Pixel::HPix2XY(resolution, hpixnum, superpixnum, x, y);
	Stomp::Pixel tmp_pix(x, y, resolution, 1.0);
	//Stomp::ScalarPixel tmp_pix(x, y, resolution, unmasked, 0, 0);
	total_area += unmasked*tmp_pix.Area();
	keep_map[tmp_pix.Pixnum()] = 1;
	n_keep++;
	//galaxy_pix.push_back(tmp_pix);
      } else {
	if ((seeing < FLAGS_seeing_min) || (seeing > FLAGS_seeing_max))
	  n_seeing++;
	if ((extinction < FLAGS_extinction_min) ||
	    (extinction > FLAGS_extinction_max)) n_extinction++;
	if ((sky >= FLAGS_sky_min) || (sky <= FLAGS_sky_max)) n_sky++;
	if (n_objects == 0) n_blank++;
      }
    }
  }
  sysmap_file.close();
  std::cout << "\tKept " << n_keep << "/" << n_pixel <<
    " systematics pixels; " << total_area << " sq. degrees...\n";
  std::cout << "\t\t" << n_seeing << " failed seeing cut.\n";
  std::cout << "\t\t" << n_extinction << " failed extinction cut.\n";
  std::cout << "\t\t" << n_sky << " failed sky_brightness cut.\n";
  std::cout << "\t\t" << n_blank << " were empty.\n";
}

Stomp::ScalarMap* LoadSysData(KeepDict& keep_map, int region_resolution, SystematicsField sys_field) {
  std::ifstream sysmap_file(FLAGS_sysmap_file.c_str());
  double unmasked, seeing, extinction, sky;
  uint32_t x, y, hpixnum, superpixnum, resolution, n_objects;
  double total_area = 0.0;
  Stomp::ScalarVector sys_pix;
  KeepDictIterator keep_iter;

  while (!sysmap_file.eof()) {
    sysmap_file >> hpixnum >> superpixnum >> resolution >> unmasked >>
      seeing >> extinction >> sky >> n_objects;

    if (!sysmap_file.eof()) {
      Stomp::Pixel::HPix2XY(resolution, hpixnum, superpixnum, x, y);
      Stomp::ScalarPixel pix(x, y, resolution, unmasked, 0.0, n_objects);
      keep_iter = keep_map.find(pix.Pixnum());
      if (keep_iter != keep_map.end()) {
	switch (sys_field) {
	case Seeing:
	  pix.SetIntensity(seeing);
	  break;
	case Extinction:
	  pix.SetIntensity(extinction);
	  break;
	case Sky:
	  pix.SetIntensity(sky);
	  break;
	}
	total_area+= unmasked*pix.Area();
	sys_pix.push_back(pix);
      }
    }
  }
  sysmap_file.close();

  std::cout<<"Systematics Area "<<total_area<<"\n";

  Stomp::ScalarMap* sys_map =
    new Stomp::ScalarMap(sys_pix, Stomp::ScalarMap::ScalarField);
  sys_pix.clear();

  sys_map->InitializeRegions(FLAGS_n_jack, region_resolution);

  return sys_map;
}

void AutoCorrelateGalaxies(Stomp::ScalarMap* galaxy_map,
			   Stomp::AngularCorrelation& wtheta) {
  // Since we're using jack knife samples, we need to initialize that
  // functionality in the AngularCorrelation object.
  for (Stomp::ThetaIterator iter=wtheta.Begin();
       iter!=wtheta.End();++iter) iter->InitializeRegions(FLAGS_n_jack);

  // And set up our AngularCorrelation resolution limits around the limits of
  // the galaxy map.
  wtheta.SetMinResolution(galaxy_map->RegionResolution());
  wtheta.SetMaxResolution(galaxy_map->Resolution());

  // Now we do the requested auto-correlation.
  int max_resolution = galaxy_map->Resolution();
  int min_resolution = wtheta.MinResolution();

  std::cout << "Starting galaxy auto-correlation...\n";
  for (Stomp::ThetaIterator iter=wtheta.Begin(max_resolution);
       iter!=wtheta.End(max_resolution);++iter)
    galaxy_map->AutoCorrelateWithRegions(iter);

  for (int resolution=max_resolution/2;
       resolution>=min_resolution;resolution/=2) {
    Stomp::ScalarMap* galaxy_sub_map =
      new Stomp::ScalarMap(*galaxy_map, resolution);
    galaxy_sub_map->InitializeRegions(*galaxy_map);

    for (Stomp::ThetaIterator iter=wtheta.Begin(resolution);
	 iter!=wtheta.End(resolution);++iter) {
      galaxy_sub_map->AutoCorrelateWithRegions(iter);
    }
    delete galaxy_sub_map;
  }
}

void CrossCorrelateSystematicsField(Stomp::ScalarMap* galaxy_map,
				    SystematicsField sys_field,
				    Stomp::AngularCorrelation& wtheta) {
  for (Stomp::ThetaIterator iter=wtheta.Begin();
       iter!=wtheta.End();++iter) iter->InitializeRegions(FLAGS_n_jack);

  wtheta.SetMinResolution(galaxy_map->RegionResolution());
  wtheta.SetMaxResolution(galaxy_map->Resolution());

  //Scan through the systematics file again and load up the appropriate values.
  Stomp::ScalarVector sys_pix;
  std::ifstream sysmap_file(FLAGS_sysmap_file.c_str());
  double unmasked, seeing, extinction, sky;
  uint32_t x, y, hpixnum, superpixnum, resolution, n_objects;

  switch (sys_field) {
  case Seeing:
    std::cout << "Reading in seeing values from " <<
      FLAGS_sysmap_file << "...\n";
    break;
  case Extinction:
    std::cout << "Reading in extinction values from " <<
      FLAGS_sysmap_file << "...\n";
    break;
  case Sky:
    std::cout << "Reading in sky brightness values from " <<
      FLAGS_sysmap_file << "...\n";
    break;
  }

  // Parse the systematics file and pull out the requested field
  while (!sysmap_file.eof()) {
    sysmap_file >> hpixnum >> superpixnum >> resolution >> unmasked >>
      seeing >> extinction >> sky >> n_objects;

    if (!sysmap_file.eof()) {
      if ((seeing >= FLAGS_seeing_min) &&
	  (seeing <= FLAGS_seeing_max) &&
	  (extinction >= FLAGS_extinction_min) &&
	  (extinction <= FLAGS_extinction_max) &&
	  (sky >= FLAGS_sky_min) &&
	  (sky <= FLAGS_sky_max) &&
	  (n_objects > 0)) {
	Stomp::Pixel::HPix2XY(resolution, hpixnum, superpixnum, x, y);
	//Stomp::ScalarPixel pix(resolution, hpixnum, superpixnum,unmasked, 0.0);
	Stomp::ScalarPixel pix(x, y, resolution, unmasked, 0.0, n_objects);
	switch (sys_field) {
	case Seeing:
	  pix.SetIntensity(seeing);
	  break;
	case Extinction:
	  pix.SetIntensity(extinction);
	  break;
	case Sky:
	  pix.SetIntensity(sky);
	  break;
	}
	sys_pix.push_back(pix);
      }
    }
  }
  sysmap_file.close();

  // Set up our systematics ScalarMap
  Stomp::ScalarMap* sys_map =
    new Stomp::ScalarMap(sys_pix, Stomp::ScalarMap::ScalarField);
  sys_pix.clear();
  sys_map->InitializeRegions(*galaxy_map);

  // Now we do the requested cross-correlation.
  int max_resolution = galaxy_map->Resolution();
  int min_resolution = wtheta.MinResolution();

  switch (sys_field) {
  case Seeing:
    std::cout << "Cross-correlating galaxies with seeing...\n";
    break;
  case Extinction:
    std::cout << "Cross-correlating galaxies with extinction...\n";
    break;
  case Sky:
    std::cout << "Cross-correlating galaxies with sky brightness...\n";
    break;
  }
  for (Stomp::ThetaIterator iter=wtheta.Begin(max_resolution);
       iter!=wtheta.End(max_resolution);++iter) {
    galaxy_map->CrossCorrelateWithRegions(*sys_map, iter);
  }

  std::cout << "Completed on galaxy map\n";

  for (uint32_t resolution=max_resolution/2;
       resolution>=min_resolution;resolution/=2) {
    std::cout << "Current Resolution: " << resolution << "\n";
    Stomp::ScalarMap* galaxy_sub_map =
      new Stomp::ScalarMap(*galaxy_map, resolution);
    galaxy_sub_map->InitializeRegions(*galaxy_map);

    Stomp::ScalarMap* sys_sub_map = 
      new Stomp::ScalarMap(*sys_map, resolution);
    sys_sub_map->InitializeRegions(*sys_map);
    for (Stomp::ThetaIterator iter=wtheta.Begin(resolution);
	 iter!=wtheta.End(resolution);++iter) {
      galaxy_sub_map->CrossCorrelateWithRegions(*sys_sub_map, iter);
    }
    delete sys_sub_map;
    delete galaxy_sub_map;
  }
}

