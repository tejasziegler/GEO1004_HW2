#pragma once
#include "json.hpp"

// Triangulates every surface of every Building's LoD2.2 geometry,
// updating boundaries and semantic surfaces consistently. Modifies `j` in place.
void triangulate_surfaces(nlohmann::json& j);

void print_triangle_stats(const nlohmann::json& j);

void check_semantic_lengths(const nlohmann::json& j);