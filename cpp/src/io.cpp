#include "io.h"     // (catches mistakes where the header doesn't declare what the .cpp defines)
#include <iostream>
#include <fstream>

#include "json.hpp"

nlohmann:: json read_cityjson(const std::string& filename) {
    std::ifstream input(filename);
    if (!input.is_open()) {
        throw std::runtime_error("Could not open file " + filename);
    }
    nlohmann:: json j;          // empty JSON object
    input >> j;
    return j;
}

void write_cityjson(const nlohmann::json& j, const std::string& filename) {
    std::ofstream out(filename);    // overwrite/create file for writing
    if (!out.is_open()) {
        throw std::runtime_error("Cannot write output file: " + filename);
    }
    out << j.dump(2) << std::endl;  // dump(2) = serialise to string with 2-space indentation

    std::cout << "Written to: " << filename << std::endl;
}

