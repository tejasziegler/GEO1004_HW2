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
void print_triangle_stats(const json& j);
void check_semantic_lengths(const json& j);
#include <CGAL/version.h>
#include <iostream>

int main(int argc, const char* argv[]) {
  const std::string filename = (argc > 1) ? argv[1] : "../nextbk_2b.city.json";
  std::cout << "CGAL version: " << CGAL_VERSION_STR << std::endl;

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

  //Triangulation + debugging functions
  triangulate_surfaces(j);
  print_triangle_stats(j);
  check_semantic_lengths(j);

  //Volume Calculation
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
