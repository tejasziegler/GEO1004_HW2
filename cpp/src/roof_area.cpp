#include <iostream>

#include "roof_area.h"
#include "json.hpp"
using json = nlohmann::json;

void add_total_roof_areas(json& j) {
  // TODO: sum the area of all surfaces whose semantic type is RoofSurface and
  // store it in attributes["geo1004_total_roof_area"].
  (void)j;
  std::cout << "Roof area calculation: TODO" << std::endl;
}
