#include <iostream>
#include <vector>
#include <cmath>
#include "volume.h"
#include "json.hpp"
using json = nlohmann::json;


// Define Point data structure
struct Point3D {
  double x;
  double y;
  double z;
};

// HELPER 1: Coordinate decomposition
Point3D get_decompressed_point(int v_idx, const json& j) {
  // 1. Get the integer triplet from the global vertex list
  // Access the raw integers from the global vertices list and create an alias vi
  auto& vi = j["vertices"][v_idx];

  // 2. similarly access the scale and translate
  auto& s = j["transform"]["scale"];
  auto& t = j["transform"]["translate"];

  // 3. apply the scale and translate logic
  double rx = (vi[0].get<int>() * s[0].get<double>()) + t[0].get<double>();
  double ry = (vi[1].get<int>() * s[1].get<double>()) + t[1].get<double>();
  double rz = (vi[2].get<int>() * s[2].get<double>()) + t[2].get<double>();

  // 3. Return the result as our struct
  return {rx, ry, rz};
}

// HELPER 2: The signed volume MATH
double signed_volume(Point3D a, Point3D b, Point3D c) {
  double det = a.x * (b.y * c.z - b.z * c.y) - b.x * (a.y * c.z - a.z * c.y) + c.x * (a.y * b.z - a.z * b.y);
  return det / 6.0;
}


void add_building_volumes(json& j) {
  // INPUTS:
  // 1. json& j: The full CityJSON object (already triangulated by your friend).
  // 2. The Geometry: Specifically j["CityObjects"][building_id]["geometry"], where type is Solid and lod is 2.2.
  // 3. The Transform: j["transform"]["scale"] and j["transform"]["translate"] (essential for real-world meters).
  //
  // OUTPUTS:
  // 1. Attribute Injection: You must write the result into j["CityObjects"][id]["attributes"]["geo1004_volume"].
  // 2. Return value: Usually void (since you modify the JSON in place) or a double if you are processing one building at a time.

  // IMPLEMENTATION STARTS

  // 'for' loop over all objects for cityJSON, 'auto' helps c++ to detect the data type and "&" lets c++ to deal with existing data and not to create a copy that might be slow
  // 'co' for each turn of loop, this represents one specific object. 'co.key()' will get the ID such as "UUID_123". and building geometry with 'co.value()'.
  for (auto& co : j["CityObjects"].items()) {
    if (!co.value().contains("type") || co.value()["type"] != "Building") continue;
    if (!co.value().contains("geometry") || co.value()["geometry"].empty()) {
      std::cout << "Skipping: " << co.key() << " (no geometry)" << std::endl;
      continue;
    }
    // Skip buildings flagged as geometrically invalid by 3DBAG's own validator
    if (co.value().contains("attributes") && co.value()["attributes"].contains("b3_val3dity_lod22")) {
        auto& val = co.value()["attributes"]["b3_val3dity_lod22"];
        
        // Check if the validation string is NOT "[]"
        // Since 3DBAG sometimes stores this as a string "[]" instead of an actual array
        if (val.is_string() && val.get<std::string>() != "[]") {
            std::cout << ">>> SKIPPING INVALID: " << co.key() << " | Errors: " << val << std::endl;
            continue;
        }
        
        // Safety: also handle it if it actually IS a real JSON array
        if (val.is_array() && !val.empty()) {
            std::cout << ">>> SKIPPING INVALID (Array): " << co.key() << " | Errors: " << val.dump() << std::endl;
            continue;
        }
    }

    double volume_co = 0.0; // variable for the total volume of this city object

    bool found_valid_geometry = false;

    // ← loop ALL geometries: after LoD filtering a building may have multiple
    // LoD2.2 Solid geometries (one from the parent Building + one per former
    // BuildingPart child). Summing all of them gives the correct total volume.
    for (auto& g : co.value()["geometry"]) {
      if (!g.contains("lod") || g["lod"] != "2.2") continue; // skip non-2.2 geometries
      if (!g.contains("type") || g["type"] != "Solid") continue; // skip non-Solid geometries

      found_valid_geometry = true;

      // implement a first shell logic: if shell[0], it is the outer boundary → add volume_shell to volume_co
      // if i != 0, it is a void/interior shell → subtract it.
      // a size_t counter in the for loop checks if counter is at 0 (outer) or not (void)
      for (size_t i = 0; i < g["boundaries"].size(); i++) { // size_t is a specific data type
        auto& shell = g["boundaries"][i]; // creates the 'shell' alias for the current index
        double volume_shell = 0.0; // variable for the total volume of this shell

        for (auto& face : shell) { // gets inside the surface; face = each surface
          auto& ring = face[0]; // grabs the outer ring = 3 indices of vertices
                                // {there might be holes, but I only care about the main outer shape (the first ring).}
          if (ring.size() < 3) continue; // sanity check: a ring must have at least 3 vertices to form a surface

          Point3D o1 = get_decompressed_point(ring[0].get<int>(), j); // pivot point: extract coordinates for 3 indices

          // This loop handles triangles (1 iteration), quads (2), pentagons (3), etc.
          for (size_t k = 1; k + 1 < ring.size(); k++) {
            Point3D o2 = get_decompressed_point(ring[k].get<int>(), j);
            Point3D o3 = get_decompressed_point(ring[k+1].get<int>(), j);

            double volume = signed_volume(o1, o2, o3); // using three coords builds volume for ring
            volume_shell += volume;
          } // triangle loop ends here

        } // face loop ends here

        // accumulate all shells of city_object
        if (i == 0) volume_co += volume_shell;
        else volume_co -= volume_shell;

      } // shell loop ends here

    } // geometry loop ends here

    if (!found_valid_geometry) {
      std::cout << "Skipping: " << co.key() << " (no lod 2.2 or solid geometry)" << std::endl;
      continue;
    }

    // Deferred std::abs(): applying only at the CityObject level keeps void subtraction
    // correct and eliminates floating-point residuals (e.g. -0.0000001).
    // Save volume to city_object attribute.
    //debug:
    // if (std::abs(volume_co) > 100000) {  // flag suspiciously large buildings
    //   std::cout << "Large volume building: " << co.key() 
    //             << " volume=" << std::abs(volume_co) << std::endl;
    // }
    co.value()["attributes"]["geo1004_volume"] = std::abs(volume_co);

  } // CityObject loop ends here

  std::cout << "Volume calculated and added as CityObject attribute: geo1004_volume" << std::endl;
}