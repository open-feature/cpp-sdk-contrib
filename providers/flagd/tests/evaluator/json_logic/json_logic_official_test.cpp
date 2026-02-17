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

  json rule = test_case[0];
  json data = test_case[1];
  json expected = test_case[2];

  std::cerr << "Running: " << rule << " on Data: " << data << std::endl;

  absl::StatusOr<json> result = json_logic_.Apply(rule, data);

  json result_json = result.ok() ? *result : nullptr;

  EXPECT_EQ(result_json, expected)
      << "Rule: " << rule << "\nData: " << data << "\nExpected: " << expected
      << "\nGot: "
      << (result.ok() ? result->dump() : result.status().ToString());
}

std::vector<json> LoadTests() {
  // This file is downloaded from: https://jsonlogic.com/tests.json
  std::string file_path =
      "providers/flagd/tests/evaluator/json_logic/tests.json";
  std::ifstream f(file_path);
  if (!f.is_open()) {
    // Fallback or error. In Bazel, runfiles might be needed, but relative path
    // often works if CWD is correct. Let's try to print error.
    std::cerr << "Could not open " << file_path << std::endl;
    return {};
  }
  json j;
  f >> j;
  std::vector<json> valid_cases;
  for (const auto& element : j) {
    if (element.is_array() && element.size() >= 3) {
      valid_cases.push_back(element);
    }
  }
  return valid_cases;
}

INSTANTIATE_TEST_SUITE_P(JsonLogicSuite, JsonLogicTest,
                         ::testing::ValuesIn(LoadTests()));
