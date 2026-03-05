#include <gtest/gtest.h>

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

#include "providers/flagd/src/evaluator/json_logic/json_logic.h"

using json_logic::JsonLogic;
using nlohmann::json;

class JsonLogicTest : public ::testing::TestWithParam<json> {
 protected:
  JsonLogic json_logic_;
};

TEST_P(JsonLogicTest, RunCase) {
  const json& test_case = GetParam();

  ASSERT_TRUE(test_case.is_array());
  ASSERT_GE(test_case.size(), 3);

  const json& rule = test_case[0];
  const json& data = test_case[1];
  const json& expected = test_case[2];

  std::cerr << "Running: " << rule << " on Data: " << data << "\n";

  absl::StatusOr<json> result = json_logic_.Apply(rule, data);

  json result_json = result.ok() ? *result : nullptr;

  EXPECT_EQ(result_json, expected)
      << "Rule: " << rule << "\nData: " << data << "\nExpected: " << expected
      << "\nGot: "
      << (result.ok() ? result->dump() : result.status().ToString());
}

std::vector<json> LoadTests() {
  // This file is generated from ./generate_tests_annotations.py
  std::string file_path =
      "providers/flagd/tests/evaluator/json_logic/tests_annotated.json";
  std::ifstream test_file(file_path);
  if (!test_file.is_open()) {
    std::cerr << "Could not open " << file_path << "\n";
    return {};
  }
  json tests_json;
  test_file >> tests_json;
  std::vector<json> valid_cases;
  if (tests_json.is_object()) {
    for (auto& [key, cases] : tests_json.items()) {
      if (cases.is_array()) {
        for (const auto& element : cases) {
          if (element.is_array() && element.size() >= 3) {
            valid_cases.push_back(element);
          }
        }
      }
    }
  }
  return valid_cases;
}

INSTANTIATE_TEST_SUITE_P(JsonLogicSuite, JsonLogicTest,
                         ::testing::ValuesIn(LoadTests()));
