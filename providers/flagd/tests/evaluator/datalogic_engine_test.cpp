#include "flagd/evaluator/datalogic_engine.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "absl/status/status.h"

namespace flagd {
namespace {

using nlohmann::json;

TEST(DatalogicEngineTest, EvaluatesVarOperator) {
  DatalogicEngine engine;
  auto result =
      engine.Apply(json::parse(R"({"var": "x"})"), json::parse(R"({"x": 42})"));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(*result, 42);
}

TEST(DatalogicEngineTest, EvaluatesFlagdStartsWithOperator) {
  DatalogicEngine engine;
  auto result = engine.Apply(
      json::parse(R"({"starts_with": [{"var": "email"}, "hello@"]})"),
      json::parse(R"({"email": "hello@example.com"})"));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(*result, true);
}

TEST(DatalogicEngineTest, EvaluatesFlagdSemVerOperator) {
  DatalogicEngine engine;
  auto result =
      engine.Apply(json::parse(R"({"sem_ver": [{"var": "v"}, ">=", "1.2.3"]})"),
                   json::parse(R"({"v": "1.2.4"})"));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(*result, true);
}

TEST(DatalogicEngineTest, FractionalProducesStableVariant) {
  DatalogicEngine engine;
  const auto rule = json::parse(R"({
    "fractional": [
      {"var": "targetingKey"},
      ["red", 50],
      ["blue", 50]
    ]
  })");
  const auto data = json::parse(R"({"targetingKey": "user-123"})");

  auto first = engine.Apply(rule, data);
  ASSERT_TRUE(first.ok()) << first.status();
  ASSERT_TRUE(first->is_string());

  auto second = engine.Apply(rule, data);
  ASSERT_TRUE(second.ok()) << second.status();
  EXPECT_EQ(*first, *second);
  EXPECT_TRUE(*first == "red" || *first == "blue");
}

TEST(DatalogicEngineTest, ReportsParseErrorAsInternal) {
  DatalogicEngine engine;
  auto result =
      engine.Apply(json::parse(R"({"not_a_real_op": []})"), json::object());
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInternal);
}

}  // namespace
}  // namespace flagd
