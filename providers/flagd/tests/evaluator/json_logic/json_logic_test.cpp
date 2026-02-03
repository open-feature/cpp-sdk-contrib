#include "providers/flagd/src/evaluator/json_logic/json_logic.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

using json_logic::JsonLogic;
using nlohmann::json;

class JsonLogicTest : public ::testing::Test {
 protected:
  JsonLogic json_logic_;
};

TEST_F(JsonLogicTest, ApplyPrimitives) {
  json data = json::object();

  EXPECT_EQ(json_logic_.Apply(json(true), data), true);
  EXPECT_EQ(json_logic_.Apply(json(false), data), false);
  EXPECT_EQ(json_logic_.Apply(json(123), data), 123);
  EXPECT_EQ(json_logic_.Apply(json(12.34), data), 12.34);
  EXPECT_EQ(json_logic_.Apply(json("string"), data), "string");
  EXPECT_EQ(json_logic_.Apply(json(nullptr), data), nullptr);
}

TEST_F(JsonLogicTest, ApplyArray) {
  json data = json::object();
  json logic = json::array({1, "two", true});

  json result = json_logic_.Apply(logic, data);

  EXPECT_TRUE(result.is_array());
  EXPECT_EQ(result.size(), 3);
  EXPECT_EQ(result[0], 1);
  EXPECT_EQ(result[1], "two");
  EXPECT_EQ(result[2], true);
}

TEST_F(JsonLogicTest, ApplyEmptyObject) {
  json data = json::object();
  json logic = json::object();

  EXPECT_EQ(json_logic_.Apply(logic, data), logic);
}

TEST_F(JsonLogicTest, ApplyUnknownOperator) {
  json data = json::object();
  json logic = json::parse(R"({"unknown_op": [1, 2]})");

  EXPECT_EQ(json_logic_.Apply(logic, data), nullptr);
}

TEST_F(JsonLogicTest, CustomOperation) {
  json_logic_.RegisterOperation(
      "custom", [](const JsonLogic&, const json& args, const json&) {
        return "custom_result";
      });

  json logic = json::parse(R"({"custom": []})");
  EXPECT_EQ(json_logic_.Apply(logic, {}), "custom_result");
}
