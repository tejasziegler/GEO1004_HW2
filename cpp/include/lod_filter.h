#pragma once
#include "json.hpp"

// Keeps only the LoD2.2 geometry: removes LoD1.2 and LoD1.3, moves the
// LoD2.2 geometry from each BuildingPart into its parent Building, and
// deletes the BuildingPart. Modifies `j` in place.
void keep_lod22_and_merge_to_buildings(nlohmann::json& j);

void print_lod_filter_debug_stats(const nlohmann::json& j, const std::string& label);