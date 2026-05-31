/*
+------------------------------------------------------------------------------+
|                                                                              |
|                                 Hugo Ledoux                                  |
|                             h.ledoux@tudelft.nl                              |
|                                  2026-05-10                                  |
|                                                                              |
+------------------------------------------------------------------------------+
*/

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <cstdlib>
#include <ctime>



//-- https://github.com/nlohmann/json
//-- used to read and write (City)JSON
#include "json.hpp" //-- it is in the /include/ folder

//-- https://github.com/nlohmann/json
//-- used to read and write (City)JSON
#include "json.hpp" //-- it is in the /include/ folder

using json = nlohmann::json;

//-- include our own functions
#include "io.h"                 // print_model_summary
#include "lod_filter.h"         // keep_lod22_and_merge_to_buildings()
#include "triangulate.h"        // triangulate_surfaces()
#include "roof_area.h"          // add_total_roof_areas()
#include "volume.h"             // add_building_volumes()

// declare helper functions (defined below)
int   get_no_roof_surfaces(const json& j);
void  list_all_vertices(const json& j);
void  visit_roofsurfaces(const json& j);
void print_lod_filter_debug_stats(const json& j, const std::string& label); // Demo/debugging helper.
void print_triangle_stats(const json& j);
void check_semantic_lengths(const json& j);

int main(int argc, const char * argv[]) {
  //-- will read the file passed as argument or twobuildings.city.json if nothing is passed
  const char* filename = (argc > 1) ? argv[1] : "/Users/tejasziegler/Documents/GEOMATICS/GEO1004_3D-model/Assignment 2/GEO1004_HW2/data/9-284-556.city.json";
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

  // list_all_vertices(j);     // disable after 1 run to check
  // visit_roofsurfaces(j);    // disable after 1 run to check

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

  print_model_summary(j);

  // =====================================================================
  //                          PIPELINE STEPS
  // =====================================================================

  std::cout << "\n=== Step 1: Filter to LoD2.2 ===" << std::endl;
  keep_lod22_and_merge_to_buildings(j);           // modifies `j` in place

  std::cout << "\n=== Step 2: Triangulate surfaces ===" << std::endl;
  triangulate_surfaces(j);            // modifies `j` in place
  print_triangle_stats(j);
  check_semantic_lengths(j);

  std::cout << "\n=== Step 3: Per-Building attributes ===" << std::endl;
  add_building_volumes(j);
  add_total_roof_areas(j);

  // // KEEPING THIS DEPENDING ON DAMAN's IMPLEMENTATION OF VOLUME AND AREAS!
  // std::srand(std::time(nullptr));                 // (legacy: was used for rand() placeholder)
  // for (auto& co : j["CityObjects"].items()) {     // .items() gives (key, value) pairs
  //   if (co.value()["type"] != "Building") continue;   // skip BuildingPart, etc.
  //
  //   const std::string& building_id = co.key();    // the CityObject's ID string
  //
  //   //-- compute the two attributes by calling our (stub) functions
  //   double volume    = add_building_volumes(j, building_id);
  //   double roof_area = add_total_roof_areas(j, building_id);
  //
  //   //-- write attributes with the exact names required by the assignment brief
  //   co.value()["attributes"]["geo1004_volume"]           = volume;
  //   co.value()["attributes"]["geo1004_total_roof_area"]  = roof_area;
  //
  //   //-- flag the (currently expected) sentinel returns from stubs so we see them clearly
  //   if (volume < 0 || roof_area < 0) {
  //     std::cerr << "  [warn] Building " << building_id
  //               << " — stub returned (volume=" << volume
  //               << ", roof_area=" << roof_area << ")" << std::endl;
  //   }



  // std::srand(std::time(nullptr));       // what is this?
  //
  // //-- add an attribute "volume"
  // for (auto& co : j["CityObjects"]) {
  //   if (co["type"] == "Building") {
  //     co["attributes"]["volume"] = rand();
  //   }
  // }




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

  std::cout << "Done." << std::endl;              // added: signals successful end of run

  return 0;
}

// ==============================================================================================

// Visit every 'RoofSurface' in the CityJSON model and output its geometry (the arrays of indices)
// Useful to learn to visit the geometry boundaries and at the same time check their semantics.
void visit_roofsurfaces(const json& j) {
  for (auto& co : j["CityObjects"].items()) {
    for (auto& g : co.value()["geometry"]) {
      if (g["type"] == "Solid") {
        for (int i = 0; i < g["boundaries"].size(); i++) {
          for (int k = 0; k < g["boundaries"][i].size(); k++) {
            int sem_index = g["semantics"]["values"][i][k];
            if (g["semantics"]["surfaces"][sem_index]["type"].get<std::string>().compare("RoofSurface") == 0) {
              std::cout << "RoofSurface: " << g["boundaries"][i][k] << std::endl;
            }
          }
        }
      }
    }
  }
}


// Returns the number of 'RooSurface' in the CityJSON model
int get_no_roof_surfaces(const json& j) {
  int total = 0;
  for (auto& co : j["CityObjects"].items()) {
    for (auto& g : co.value()["geometry"]) {
      if (g["type"] == "Solid") {
        for (auto& shell : g["semantics"]["values"]) {
          for (auto& s : shell) {
            if (g["semantics"]["surfaces"][s.get<int>()]["type"].get<std::string>().compare("RoofSurface") == 0) {
              total += 1;
            }
          }
        }
      }
    }
  }
  return total;
}


// CityJSON files have their vertices compressed: https://www.cityjson.org/specs/1.1.1/#transform-object
// this function visits all the surfaces and print the (x,y,z) coordinates of each vertex encountered
void list_all_vertices(const json& j) {
  for (auto& co : j["CityObjects"].items()) {
    std::cout << "= CityObject: " << co.key() << std::endl;
    for (auto& g : co.value()["geometry"]) {
      if (g["type"] == "Solid") {
        for (auto& shell : g["boundaries"]) {
          for (auto& surface : shell) {
            for (auto& ring : surface) {
              std::cout << "---" << std::endl;
              for (auto& v : ring) {
                std::vector<int> vi = j["vertices"][v.get<int>()];
                double x = (vi[0] * j["transform"]["scale"][0].get<double>()) + j["transform"]["translate"][0].get<double>();
                double y = (vi[1] * j["transform"]["scale"][1].get<double>()) + j["transform"]["translate"][1].get<double>();
                double z = (vi[2] * j["transform"]["scale"][2].get<double>()) + j["transform"]["translate"][2].get<double>();
                std::cout << std::setprecision(2) << std::fixed << v << " (" << x << ", " << y << ", " << z << ")" << std::endl;
              }
            }
          }
        }
      }
    }
  }
}

void check_semantic_lengths(const json& j) {
  for (const auto& item : j["CityObjects"].items()) {
    const json& co = item.value();

    if (!co.contains("type") || co["type"] != "Building") {
      continue;
    }

    for (const auto& geom : co["geometry"]) {
      if (!geom.contains("boundaries") || !geom.contains("semantics")) {
        continue;
      }

      for (size_t shell_id = 0; shell_id < geom["boundaries"].size(); shell_id++) {
        size_t n_surfaces = geom["boundaries"][shell_id].size();
        size_t n_values = geom["semantics"]["values"][shell_id].size();

        if (n_surfaces != n_values) {
          std::cout << "Semantic mismatch in shell " << shell_id
                    << ": surfaces=" << n_surfaces
                    << ", values=" << n_values << std::endl;
        }
      }
    }
  }
}

void print_triangle_stats(const json& j) {
  int surfaces = 0;
  int triangles = 0;
  int non_triangles = 0;

  for (const auto& item : j["CityObjects"].items()) {
    const std::string building_id = item.key();
    const json& co = item.value();

    if (!co.contains("type") || co["type"] != "Building") {
      continue;
    }

    if (!co.contains("geometry")) {
      continue;
    }

    for (size_t geom_id = 0; geom_id < co["geometry"].size(); geom_id++) {
      const json& geom = co["geometry"][geom_id];

      if (!geom.contains("boundaries")) {
        continue;
      }

      for (size_t shell_id = 0; shell_id < geom["boundaries"].size(); shell_id++) {
        const json& shell = geom["boundaries"][shell_id];

        for (size_t surface_id = 0; surface_id < shell.size(); surface_id++) {
          const json& surface = shell[surface_id];

          surfaces++;

          bool is_triangle =
              surface.size() == 1 &&
              surface[0].is_array() &&
              surface[0].size() == 3;

          if (is_triangle) {
            triangles++;
          } else {
            non_triangles++;

            std::cout << "Non-triangle surface found:" << std::endl;
            std::cout << "  Building ID: " << building_id << std::endl;
            std::cout << "  Geometry ID: " << geom_id << std::endl;
            std::cout << "  Shell ID: " << shell_id << std::endl;
            std::cout << "  Surface ID: " << surface_id << std::endl;
            std::cout << "  Number of rings: " << surface.size() << std::endl;

            for (size_t ring_id = 0; ring_id < surface.size(); ring_id++) {
              std::cout << "  Ring " << ring_id
                        << " vertex count: " << surface[ring_id].size()
                        << std::endl;
            }

            std::cout << "  Surface JSON: " << surface.dump() << std::endl;
          }
        }
      }
    }
  }

  std::cout << "=== Triangulation Stats ===" << std::endl;
  std::cout << "Surfaces after triangulation: " << surfaces << std::endl;
  std::cout << "Triangle surfaces: " << triangles << std::endl;
  std::cout << "Non-triangle surfaces: " << non_triangles << std::endl;
}


// Demo/debugging helper: print stats that show whether LoD filtering worked.
void print_lod_filter_debug_stats(const json& j, const std::string& label) {
  int buildings = 0;
  int building_parts = 0;
  int lod22_geometries_in_buildings = 0;
  int non_lod22_geometries_in_buildings = 0;
  int buildings_still_having_children = 0;

  for (const auto& item : j["CityObjects"].items()) {
    const json& co = item.value();

    if (!co.contains("type")) {
      continue;
    }

    if (co["type"] == "Building") {
      buildings += 1;

      if (co.contains("children") && !co["children"].empty()) {
        buildings_still_having_children += 1;
      }

      if (co.contains("geometry")) {
        for (const auto& geom : co["geometry"]) {
          if (geom.contains("lod") && geom["lod"] == "2.2") {
            lod22_geometries_in_buildings += 1;
          } else {
            non_lod22_geometries_in_buildings += 1;
          }
        }
      }
    } else if (co["type"] == "BuildingPart") {
      building_parts += 1;
    }
  }


  std::cout << std::endl;
  std::cout << "=== " << label << " ===" << std::endl;
  std::cout << "Buildings: " << buildings << std::endl;
  std::cout << "BuildingParts: " << building_parts << std::endl;
  std::cout << "LoD2.2 geometries in Buildings: " << lod22_geometries_in_buildings << std::endl;
  std::cout << "Non-LoD2.2 geometries in Buildings: " << non_lod22_geometries_in_buildings << std::endl;
  std::cout << "Buildings still having children: " << buildings_still_having_children << std::endl;
  std::cout << std::endl;
}
