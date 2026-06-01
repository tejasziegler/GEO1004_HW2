#include <iostream>

#include "roof_area.h"              // this is what main.cpp calls, so by including here we can catch bugs!
#include "json.hpp"
using json = nlohmann::json;        // safe to include in cpp files (because these are leaf files, don't add to .h files)


std::array<double, 3> decompress_vertex(int idx, json& j) {
  std::vector<int> v_i = j["vertices"][idx];
  double x = v_i[0] * j["transform"]["scale"][0].get<double>()
                              + j["transform"]["translate"][0].get<double>();
  double y = v_i[1] * j["transform"]["scale"][1].get<double>()
                    + j["transform"]["translate"][1].get<double>();
  double z = v_i[2] * j["transform"]["scale"][2].get<double>()
                    + j["transform"]["translate"][2].get<double>();
  return std::array<double, 3>{x, y, z};
}

void add_total_roof_areas(json& j) {
  for (auto& city_obj: j["CityObjects"].items()) {
    if (city_obj.value()["type"] != "Building") continue;

    double total_area = 0.0;

    for (auto& geom : city_obj.value()["geometry"]) {
      if (geom["type"] != "Solid") continue;

      for (int i = 0; i < geom["boundaries"].size(); i++) {                       // enumerate boundaries (shells)
        for (int k = 0; k < geom["boundaries"][i].size(); k++) {                  // enumerate surfaces (in shells)

          int sem_index = geom["semantics"]["values"][i][k];
          std::string type = geom["semantics"]["surfaces"][sem_index]["type"]
          .get<std::string>();

          if (type != "RoofSurface") continue;
          // ======================= Roof surfaces ==================================
          auto& ring = geom["boundaries"][i][k][0];

          if (ring.size() != 3) {
            std::cerr << "Warning: non-triangle RoofSurface ring skipped (size="
                      << ring.size() << "). Triangulation may be incomplete." << std::endl;
            continue;
          }

          auto p1 = decompress_vertex(ring[0].get<int>(), j);
          auto p2 = decompress_vertex(ring[1].get<int>(), j);
          auto p3 = decompress_vertex(ring[2].get<int>(), j);

          // --- area = 0.5 * || (p2-p1) × (p3-p1) || ---
          // cross product of edge vectors e1 = p2-p1 and e2 = p3-p1:
          double e1x = p2[0]-p1[0],  e1y = p2[1]-p1[1],  e1z = p2[2]-p1[2];
          double e2x = p3[0]-p1[0],  e2y = p3[1]-p1[1],  e2z = p3[2]-p1[2];

          double cx = e1y*e2z - e1z*e2y;   // cross product x
          double cy = e1z*e2x - e1x*e2z;   // cross product y
          double cz = e1x*e2y - e1y*e2x;   // cross product z

          double area = 0.5 * std::sqrt(cx*cx + cy*cy + cz*cz);
          total_area += area;
          // ========================================================================
        }
      }
    }
    city_obj.value()["attributes"]["geo1004_total_roof_area"] = total_area;
  }
  std::cout << "Roof area calculated and added as CityObject attribute: geo1004_total_roof_area" << std::endl;
}
//
// // Visit every 'RoofSurface' in the CityJSON model and output its geometry (the arrays of indices)
// // Useful to learn to visit the geometry boundaries and at the same time check their semantics.
// void visit_roofsurfaces1(const json& j) {
//   for (auto& co : j["CityObjects"].items()) {             // key-value pair named "co"
//     for (auto& g : co.value()["geometry"]) {                       // geometry
//       if (g["type"] == "Solid") {
//         for (int i = 0; i < g["boundaries"].size(); i++) {                          // enumerate boundaries (shell index i)
//           for (int k = 0; k < g["boundaries"][i].size(); k++) {                     // enumerate surface (index k)
//             int sem_index = g["semantics"]["values"][i][k];                      // declare semantic_index (reuse!)
//             if (g["semantics"]["surfaces"][sem_index]["type"]
//               .get<std::string>().compare("RoofSurface") == 0) {     // test whether current element is roof-surface!
//               // ===================== Roof surface level =========================
//
//               std::cout << "RoofSurface: " << g["boundaries"][i][k] << std::endl;
//
//               // list_all_vertices -----> use to calculate roof area using formula (make a helper function)
//               //
//
//
//               // ================================================================
//             }
//           }
//         }
//       }
//     }
//   }
// }
//
//
// // Returns the number of 'RooSurface' in the CityJSON model
// int get_no_roof_surfaces1(const json& j) {
//   int total = 0;
//   for (auto& co : j["CityObjects"].items()) {
//     for (auto& g : co.value()["geometry"]) {
//       if (g["type"] == "Solid") {
//         for (auto& shell : g["semantics"]["values"]) {
//           for (auto& s : shell) {
//             if (g["semantics"]["surfaces"][s.get<int>()]["type"].get<std::string>().compare("RoofSurface") == 0) {
//               total += 1;
//             }
//           }
//         }
//       }
//     }
//   }
//   return total;
// }
//
//
// // CityJSON files have their vertices compressed: https://www.cityjson.org/specs/1.1.1/#transform-object
// // this function visits all the surfaces and print the (x,y,z) coordinates of each vertex encountered
// void list_all_vertices1(const json& j) {
//   for (auto& co : j["CityObjects"].items()) {
//     std::cout << "= CityObject: " << co.key() << std::endl;
//     for (auto& g : co.value()["geometry"]) {
//       if (g["type"] == "Solid") {
//         for (auto& shell : g["boundaries"]) {
//           for (auto& surface : shell) {
//             for (auto& ring : surface) {
//               std::cout << "---" << std::endl;
//               for (auto& v : ring) {
//                 std::vector<int> vi = j["vertices"][v.get<int>()];
//                 double x = (vi[0] * j["transform"]["scale"][0].get<double>()) + j["transform"]["translate"][0].get<double>();             // decompression
//                 double y = (vi[1] * j["transform"]["scale"][1].get<double>()) + j["transform"]["translate"][1].get<double>();             // decompression
//                 double z = (vi[2] * j["transform"]["scale"][2].get<double>()) + j["transform"]["translate"][2].get<double>();             // decompression
//                 std::cout << std::setprecision(2) << std::fixed << v << " (" << x << ", " << y << ", " << z << ")" << std::endl;
//               }
//             }
//           }
//         }
//       }
//     }
//   }
// }
