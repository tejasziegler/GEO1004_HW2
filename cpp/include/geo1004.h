#ifndef GEO1004_HW2_GEO1004_H
#define GEO1004_HW2_GEO1004_H

#include <string>

#include "json.hpp"

using json = nlohmann::json;

bool read_cityjson(const std::string& filename, json& j);
bool write_cityjson(const std::string& filename, const json& j);
std::string make_output_filename(const std::string& input_filename);

void keep_lod22_and_merge_to_buildings(json& j);
void triangulate_surfaces(json& j);
void add_building_volumes(json& j);
void add_total_roof_areas(json& j);

void print_model_summary(const json& j);

#endif
