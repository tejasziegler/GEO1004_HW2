#include <iostream>
#include <vector>
#include <string>
#include "json.hpp"
#include "lod_filter.h"

void keep_lod22_and_merge_to_buildings(json& j) {
    // TODO: keep only LoD2.2 geometries, move them from BuildingPart objects to their parent Building objects, then remove the BuildingPart objects.
  for (auto& item : j["CityObjects"].items()) {
    json& co = item.value();

    if (co.contains("type") && co["type"] == "Building") {
      json new_geometries = json::array();

      if (co.contains("children")) {
        for (const auto& child_id_json : co["children"]) {
          std::string child_id = child_id_json.get<std::string>();

          if (!j["CityObjects"].contains(child_id)) {
            continue;
          }

          json& child = j["CityObjects"][child_id];

          if (child.contains("type") &&
              child["type"] == "BuildingPart" &&
              child.contains("geometry")) {

            for (const auto& geom : child["geometry"]) {
              if (geom.contains("lod") && geom["lod"] == "2.2") {
                new_geometries.push_back(geom);
              }
            }
          }
        }
      }

      co["geometry"] = new_geometries;
      co.erase("children");
    }
  }

  std::vector<std::string> to_delete;

  for (const auto& item : j["CityObjects"].items()) {
    const std::string id = item.key();
    const json& co = item.value();

    if (co.contains("type") && co["type"] == "BuildingPart") {
      to_delete.push_back(id);
    }
  }

  for (const std::string& id : to_delete) {
    j["CityObjects"].erase(id);
  }

  std::cout << "LoD2.2 filtering complete." << std::endl;
}

