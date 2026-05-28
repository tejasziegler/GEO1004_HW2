#include <iostream>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <string>
#include <vector>

#include "geo1004.h"

int get_no_roof_surfaces(const json& j); // Demo/debugging helper.
void visit_roofsurfaces(const json& j);  // Demo/debugging helper.
void list_all_vertices(const json& j);   // Demo/debugging helper.
void print_lod_filter_debug_stats(const json& j, const std::string& label); // Demo/debugging helper.

int main(int argc, const char* argv[]) {
  const std::string filename = (argc > 1) ? argv[1] : "../test_lod.city.json";

  std::cout << "Processing: " << filename << std::endl;

  json j;
  if (!read_cityjson(filename, j)) {
    return 1;
  }

  print_model_summary(j);

  //-- Demo/debugging: count RoofSurface semantics in the input model.
  int noroofsurfaces = get_no_roof_surfaces(j);
  std::cout << "Total RoofSurface: " << noroofsurfaces << std::endl;

  //-- Demo/debugging: print all vertex coordinates encountered in Solid geometries.
  // list_all_vertices(j);

  //-- Demo/debugging: print each RoofSurface boundary from Solid geometries.
  // visit_roofsurfaces(j);

  //-- Demo/debugging placeholder: assigns random volume values, not the assignment solution.
  // std::srand(std::time(nullptr));
  // for (auto& co : j["CityObjects"]) {
  //   if (co["type"] == "Building") {
  //     co["attributes"]["volume"] = rand();
  //   }
  // }

  //-- Demo/debugging: inspect model structure before LoD filtering.
  print_lod_filter_debug_stats(j, "Before LoD filtering");

  keep_lod22_and_merge_to_buildings(j);

  //-- Demo/debugging: inspect model structure after LoD filtering.
  print_lod_filter_debug_stats(j, "After LoD filtering");

  triangulate_surfaces(j);
  add_building_volumes(j);
  add_total_roof_areas(j);

  const std::string outfile = make_output_filename(filename);
  if (!write_cityjson(outfile, j)) {
    return 1;
  }

  std::cout << "Written to: " << outfile << std::endl;
  return 0;
}

// Debugging and demo function definitions after this line:

// Demo/debugging helper: visit every RoofSurface in the CityJSON model and print its boundary indices.
void visit_roofsurfaces(const json& j) {
  for (auto& co : j["CityObjects"].items()) {
    for (auto& g : co.value()["geometry"]) {
      if (g["type"] == "Solid" &&
          g.contains("semantics") &&
          g["semantics"].contains("values") &&
          g["semantics"].contains("surfaces")) {
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

// Demo/debugging helper: count RoofSurface semantic entries in Solid geometries.
int get_no_roof_surfaces(const json& j) {
  int total = 0;
  for (auto& co : j["CityObjects"].items()) {
    for (auto& g : co.value()["geometry"]) {
      if (g["type"] == "Solid" &&
          g.contains("semantics") &&
          g["semantics"].contains("values") &&
          g["semantics"].contains("surfaces")) {
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

// Demo/debugging helper: print the transformed coordinates of each vertex encountered in Solid geometries.
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
