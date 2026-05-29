#pragma once
#include "json.hpp"

// Computes the volume of each Building's LoD2.2 geometry (signed-tetrahedra
// method) and stores it as the attribute "geo1004_volume". Modifies `j` in place.
void add_building_volumes(nlohmann::json& j);