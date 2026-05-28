#include <fstream>
#include <iostream>
#include <string>

#include "geo1004.h"

bool read_cityjson(const std::string& filename, json& j) {
  std::ifstream input(filename);
  if (!input.is_open()) {
    std::cerr << "Error: cannot open " << filename << std::endl;
    return false;
  }

  input >> j;
  return true;
}

bool write_cityjson(const std::string& filename, const json& j) {
  std::ofstream output(filename);
  if (!output.is_open()) {
    std::cerr << "Error: cannot write " << filename << std::endl;
    return false;
  }

  output << j.dump(2) << std::endl;
  return true;
}

std::string make_output_filename(const std::string& input_filename) {
  std::string output_filename = input_filename;
  const std::string suffix = ".city.json";
  const size_t pos = output_filename.rfind(suffix);

  if (pos == std::string::npos) {
    return "out.city.json";
  }

  output_filename.insert(pos, "_out");
  return output_filename;
}

void print_model_summary(const json& j) {
  int buildings = 0;
  for (const auto& co : j["CityObjects"]) {
    if (co["type"] == "Building") {
      buildings += 1;
    }
  }

  std::cout << "Buildings: " << buildings << std::endl;
  std::cout << "Vertices: " << j["vertices"].size() << std::endl;
}
