#pragma once
#include "json.hpp"

// Prints summary statistics about the model (number of CityObjects,
// Buildings, vertices, RoofSurfaces) to standard output. Read-only.
void print_model_summary(const nlohmann::json& j);

int get_no_roof_surfaces(const nlohmann::json& j);