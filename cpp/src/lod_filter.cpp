#include <iostream>
#include <vector>
#include <string>
#include <map>
#include "json.hpp"
#include "lod_filter.h"
using json = nlohmann::json;

void keep_lod22_and_merge_to_buildings(json& j) {
    // 1. COLLECT: Find all geometries without modifying the JSON structure yet
    std::map<std::string, json> new_geoms_per_building;

    for (auto& item : j["CityObjects"].items()) {
        const std::string& id = item.key();
        json& co = item.value();

        // Use .value() for safer lookups
        if (co.value("type", "") != "Building") continue;

        json new_geometries = json::array();

        // Get geometry from the Building itself
        if (co.contains("geometry")) {
            for (const auto& geom : co["geometry"]) {
                if (geom.value("lod", "") == "2.2")
                    new_geometries.push_back(geom);
            }
        }

        // Get geometry from children (BuildingParts)
        if (co.contains("children")) {
            for (const auto& child_id_json : co["children"]) {
                std::string child_id = child_id_json.get<std::string>();
                if (!j["CityObjects"].contains(child_id)) continue;

                const json& child = j["CityObjects"][child_id];
                if (child.value("type", "") != "BuildingPart") continue;

                if (child.contains("geometry")) {
                    for (const auto& geom : child["geometry"]) {
                        if (geom.value("lod", "") == "2.2")
                            new_geometries.push_back(geom);
                    }
                }
            }
        }
        // Store for the second pass
        new_geoms_per_building[id] = std::move(new_geometries);
    }

    // 2. APPLY: Now modify the Building objects
    for (auto& [id, new_geoms] : new_geoms_per_building) {
        j["CityObjects"][id]["geometry"] = std::move(new_geoms);
        j["CityObjects"][id].erase("children");
    }

    // 3. DELETE: Remove the BuildingPart objects (your existing logic is fine here)
    std::vector<std::string> to_delete;
    for (const auto& item : j["CityObjects"].items()) {
        if (item.value().value("type", "") == "BuildingPart") {
            to_delete.push_back(item.key());
        }
    }

    for (const std::string& id : to_delete) {
        j["CityObjects"].erase(id);
    }

    std::cout << "LoD2.2 filtering complete." << std::endl;
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
    std::cout << label << std::endl;
    std::cout << "Buildings: " << buildings << std::endl;
    std::cout << "BuildingParts: " << building_parts << std::endl;
    std::cout << "LoD2.2 geometries in Buildings: " << lod22_geometries_in_buildings << std::endl;
    std::cout << "Non-LoD2.2 geometries in Buildings: " << non_lod22_geometries_in_buildings << std::endl;
    std::cout << "Buildings still having children: " << buildings_still_having_children << std::endl;
}