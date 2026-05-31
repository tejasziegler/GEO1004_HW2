#include <iostream>
#include <vector>
#include <cmath>
#include "geo1004.h"
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
  // TODO: compute each Building volume from signed tetrahedra and store it in
  // attributes["geo1004_volume"].


  /* 
  INPUTS: 
  1. json& j: The full CityJSON object (already triangulated by your friend).
  2. The Geometry: Specifically j["CityObjects"][building_id]["geometry"], where type is Solid and lod is 2.2.
  3. The Transform: j["transform"]["scale"] and j["transform"]["translate"] (essential for real-world meters).

  OUTPUTS:
  1. Attribute Injection: You must write the result into j["CityObjects"][id]["attributes"]["geo1004_volume"].
  2. Return value: Usually void (since you modify the JSON in place) or a double if you are processing one building at a time.
  */

  // IMPLEMENTATION STARTS

  // 'for' loop over all objects for cityJSON, 'auto' helps c++ to detect the data type and "&" lets c++ to deal with existing data and not to create a copy that might be slow
  // 'co' for each turn of loop, this represents one specific object. 'co.key()' will get the ID suchas "UUID_123". and building geometry with 'co.value()'.
  std::cout << "=== Volume Calculation ===" << std::endl;
  for (auto& co : j["CityObjects"].items()) {
    double volume_co = 0.0; // variable for the total volume of this city object

    if (co.value().contains("geometry") && !co.value()["geometry"].empty()) {
      auto& g = co.value()["geometry"][0]; // assigns g to geometries and first geometry available
    
      if (g["lod"] == "2.2" && g["type"] == "Solid") { // checks the first available geomtery if its lod 2.2, and is solid
    
        // implemet a first shell logic, if its shell[0], then its outer boundary
        // than add volume_shell to volume_co
        // if i!=0, then subtract it.
        // to implemet this use a counter in for loop, to check if counter at 0, outer, otherwise vice versa
        for (size_t i = 0; i < g["boundaries"].size(); i++) { // size_t is a specific data type
          auto& shell = g["boundaries"][i]; // creates the 'shell' alias for the current index
          double volume_shell = 0.0; // variable for the total volume of this shell
    
          for (auto& face : shell) { // gets inside the surface; face = each ring
            auto& ring = face[0]; // grabs each ring = 3 indices of vertices {there might be holes, but I only care about the main outer shape (the first ring).}
            Point3D o1 = get_decompressed_point(ring[0].get<int>(), j); // extract coordinates for 3 indices
            Point3D o2 = get_decompressed_point(ring[1].get<int>(), j);
            Point3D o3 = get_decompressed_point(ring[2].get<int>(), j);
            double volume = signed_volume(o1, o2, o3); // using three coords builds volume for ring
            volume_shell += volume;
          } // face loop ends here
          // accumulate all shells of city_object 
          if (i == 0) volume_co += volume_shell;
          else volume_co -= volume_shell;
        } // shell loop ends here
        co.value()["attributes"]["geo1004_volume"] = std::abs(volume_co); // save volume to city_object attribute
      }
      else {std::cout << "Skipping: " << co.key() << " (no lod 2.2 or solid geometry)" << std::endl;}
    }
    else {std::cout << "Skipping: " << co.key() << " (no geometry)" << std::endl;}
  }
  std::cout << "=== Volumes Successfully Calculated ===" << std::endl;
}