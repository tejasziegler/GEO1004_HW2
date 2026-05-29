#include <iostream>
#include "volume.h"
#include "json.hpp"
using json = nlohmann::json;

void add_building_volumes(json& j) {
  // TODO: compute each Building volume from signed tetrahedra and store it in
  // attributes["geo1004_volume"].
  (void)j;
  std::cout << "Volume calculation: TODO" << std::endl;
}
