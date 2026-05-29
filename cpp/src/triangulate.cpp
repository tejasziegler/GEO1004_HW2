#include <iostream>

#include "triangulate.h"
#include "json.hpp"

using json = nlohmann::json;

void triangulate_surfaces(json& j) {
  // TODO: triangulate each LoD2.2 surface while preserving semantic surface
  // indices and ring orientation.
  (void)j;
  std::cout << "Triangulation: TODO" << std::endl;
}
