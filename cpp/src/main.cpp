#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <cstdlib>
#include <ctime>

//-- https://github.com/nlohmann/json
//-- used to read and write (City)JSON
#include "json.hpp" //-- it is in the /include/ folder
using json = nlohmann::json;

//-- include our own functions
#include "io.h"                 // print_model_summary(), get_no_roof_surfaces()
#include "lod_filter.h"         // keep_lod22_and_merge_to_buildings()
#include "triangulate.h"        // triangulate_surfaces(), print_triangle_stats(), check_semantic_lengths()
#include "roof_area.h"          // add_total_roof_areas()
#include "volume.h"             // add_building_volumes()


int main(int argc, const char * argv[]) {
  //-- will read the file passed as argument or twobuildings.city.json if nothing is passed
  const char* filename = (argc > 1) ? argv[1] : "../9-284-556.city.json";
  std::cout << "Processing: " << filename << std::endl;

  std::ifstream input(filename);            // open the input file for reading
  if (!input.is_open()) {                   // guard: did it actually open?
    std::cerr << "Error: cannot open " << filename << std::endl;
    return 1;
  }
  json j;                                   // empty JSON object
  input >> j;                               // parse the file's text into 'j'
  input.close();                            // explicitly close

  std::cout << "File read successfully." << std::endl;

  //-- get total number of RoofSurface in the file
  int noroofsurfaces = get_no_roof_surfaces(j);
  std::cout << "Total RoofSurface: " << noroofsurfaces << std::endl;

  //-- print out the number of Buildings in the file
  int nobuildings = 0;
  for (auto& co : j["CityObjects"]) {       // loop over CityObject
    if (co["type"] == "Building") {                     // count those type Building
      nobuildings += 1;
    }
  }
  std::cout << "There are " << nobuildings << " Buildings in the file" << std::endl;

  //-- print out the number of vertices in the file
  std::cout << "Number of vertices " << j["vertices"].size() << std::endl;

  // =====================================================================
  //                          PIPELINE STEPS
  // =====================================================================

  std::cout << "\n=== Step 1: Filter to LoD2.2 ===" << std::endl;
  print_lod_filter_debug_stats(j, "Before LoD filtering:");
  keep_lod22_and_merge_to_buildings(j);           // modifies `j` in place
  print_lod_filter_debug_stats(j, "After LoD filtering:");


  std::cout << "\n=== Step 2: Triangulate Surfaces ===" << std::endl;
  triangulate_surfaces(j);            // modifies `j` in place
  print_triangle_stats(j);
  check_semantic_lengths(j);

  std::cout << "\n=== Step 3: Calculate Building Volumes ===" << std::endl;
  add_building_volumes(j);
  std::cout << "\n=== Step 4: Calculate Total Roof Area ===" << std::endl;
  add_total_roof_areas(j);

  //-- write to disk the modified city model (insert "_out" before ".city.json")
  std::string outfile = filename;
  size_t pos = outfile.rfind(".city.json");
  if (pos != std::string::npos) {
    outfile.insert(pos, "_out");
  } else {
    outfile = "out.city.json";
  }
  std::ofstream o(outfile);
  if (!o.is_open()) {
    std::cerr << "Error: cannot write " << outfile << std::endl;
    return 1;
  }
  std::cout << "Written to: " << outfile << std::endl;
  o << j.dump(2) << std::endl;
  o.close();

  std::cout << "Done." << std::endl;     // added: signals successful end of run

  return 0;
}
// =============================================================================







