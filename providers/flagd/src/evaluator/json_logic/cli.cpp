#include <exception>
#include <iostream>
#include <string>

#include "absl/status/status.h"
#include "json_logic.h"
#include "nlohmann/json.hpp"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " '<logic_json>' '<data_json>'\n";
    return 1;
  }

  std::string logic_str = argv[1];
  std::string data_str = argv[2];

  nlohmann::json logic;
  nlohmann::json data;
  try {
    logic = nlohmann::json::parse(logic_str);
  } catch (const std::exception& e) {
    std::cerr << "Error parsing logic JSON: " << e.what() << "\n";
    return 1;
  }

  try {
    data = nlohmann::json::parse(data_str);
  } catch (const std::exception& e) {
    std::cerr << "Error parsing data JSON: " << e.what() << "\n";
    return 1;
  }

  try {
    json_logic::JsonLogic evaluator;
    auto result = evaluator.Apply(logic, data);

    if (!result.ok()) {
      std::cerr << "Apply error: " << result.status().ToString() << "\n";
      return 1;
    }

    std::cout << result.value().dump() << "\n";
  } catch (const std::exception& e) {
    std::cerr << "Exception during evaluation: " << e.what() << "\n";
    return 2;
  } catch (...) {
    std::cerr << "Unknown exception during evaluation.\n";
    return 2;
  }

  return 0;
}
