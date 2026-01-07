#include "providers/flagd/src/evaluator/evaluator.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

using flagd::Evaluator;
using nlohmann::json;

class EvaluatorTest : public ::testing::Test {
 protected:
  Evaluator evaluator_;
};

TEST_F(EvaluatorTest, EvaluatePrimitives) {
  json data = json::object();

  EXPECT_EQ(evaluator_.Evaluate(json(true), data), true);
  EXPECT_EQ(evaluator_.Evaluate(json(false), data), false);
  EXPECT_EQ(evaluator_.Evaluate(json(123), data), 123);
  EXPECT_EQ(evaluator_.Evaluate(json(12.34), data), 12.34);
  EXPECT_EQ(evaluator_.Evaluate(json("string"), data), "string");
  EXPECT_EQ(evaluator_.Evaluate(json(nullptr), data), nullptr);
}

TEST_F(EvaluatorTest, EvaluateArray) {
  json data = json::object();
  json logic = json::array({1, "two", true});

  json result = evaluator_.Evaluate(logic, data);

  EXPECT_TRUE(result.is_array());
  EXPECT_EQ(result.size(), 3);
  EXPECT_EQ(result[0], 1);
  EXPECT_EQ(result[1], "two");
  EXPECT_EQ(result[2], true);
}

TEST_F(EvaluatorTest, EvaluateEmptyObject) {
  json data = json::object();
  json logic = json::object();

  EXPECT_EQ(evaluator_.Evaluate(logic, data), logic);
}

TEST_F(EvaluatorTest, EvaluateUnknownOperator) {
  json data = json::object();
  json logic = json::parse(R"({"unknown_op": [1, 2]})");

  EXPECT_EQ(evaluator_.Evaluate(logic, data), nullptr);
}

TEST_F(EvaluatorTest, CustomOperation) {
  evaluator_.RegisterOperation(
      "custom", [](const Evaluator&, const json& args, const json&) {
        return "custom_result";
      });

  json logic = json::parse(R"({"custom": []})");
  EXPECT_EQ(evaluator_.Evaluate(logic, {}), "custom_result");
}
