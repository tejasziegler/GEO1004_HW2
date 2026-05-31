#pragma once
#include "json.hpp"

// Computes the total RoofSurface area of each Building and stores it as the
// attribute "geo1004_total_roof_area". Modifies `j` in place.
void add_total_roof_areas(nlohmann::json& j);