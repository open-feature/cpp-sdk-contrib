#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "providers/flagd/src/evaluator/json_logic/json_logic.h"

using json_logic::JsonLogic;
using nlohmann::json;

// TODO(#38): These tests are temporary. In a subsequent PR, official tests will
// be used and these will be removed.
class OpsTest : public ::testing::Test {
 protected:
  JsonLogic json_logic_;
};

// --- VAR Operation Tests ---

TEST_F(OpsTest, VarWholeData) {
  json data = {{"key", "value"}};

  // If args are empty, return whole data
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"var": []})"), data), data);
  // If path is empty, return whole data
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"var": ""})"), data), data);
  // If the first argument is null, return whole data.
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"var": null})"), data), data);
  // Check syntactic sugar
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"var": [null]})"), data), data);
}

TEST_F(OpsTest, VarBasicAccess) {
  json data = {{"a", 1}, {"b", "test"}};

  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"var": "a"})"), data), 1);
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"var": ["a"]})"), data), 1);
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"var": "b"})"), data), "test");
}

TEST_F(OpsTest, VarNestedAccess) {
  int const age = 30;
  json data = {{"user", {{"name", "John"}, {"age", age}}}};

  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"var": "user.name"})"), data),
            "John");
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"var": "user.age"})"), data),
            age);
}

TEST_F(OpsTest, VarDefaultValue) {
  json data = {{"a", 1}};

  // Key missing, returns default
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"var": ["b", 99]})"), data), 99);

  // Key missing and no default provided, returns null
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"var": "b"})"), data), nullptr);
}

// --- AND Operation Tests ---

TEST_F(OpsTest, AndBasicBoolean) {
  json data = {};

  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"and": [true, true]})"), data),
            true);
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"and": [true, false]})"), data),
            false);
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"and": [false, true]})"), data),
            false);
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"and": [false, false]})"), data),
            false);
}

TEST_F(OpsTest, AndShortCircuit) {
  json data = {};
  // If first is false, return first (false)
  EXPECT_EQ(
      json_logic_.Apply(json::parse(R"({"and": [false, "ignored"]})"), data),
      false);

  // If all are truthy, return last
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"and": [true, [1,2], "value"]})"),
                              data),
            "value");
}

TEST_F(OpsTest, AndTruthyFalsy) {
  json data = {};

  // 0 is falsy
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"and": [1, 2]})"), data), 2);
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"and": [0, 2]})"), data), 0);

  // Empty string is falsy
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"and": ["", "value"]})"), data),
            "");
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"and": ["a", "value"]})"), data),
            "value");

  // Empty array is falsy
  EXPECT_EQ(json_logic_.Apply(json::parse(R"({"and": [[], "value"]})"), data),
            json::array());
}

TEST_F(OpsTest, AndEmpty) {
  // If values empty, returns values (empty array)
  json logic = json::parse(R"({"and": []})");
  EXPECT_EQ(json_logic_.Apply(logic, {}), json::array());
}

// --- Nested Operations Tests ---

TEST_F(OpsTest, AndWithVar) {
  int const age = 25;
  json data = {{"is_admin", true}, {"is_active", true}, {"age", age}};

  // Both true
  json logic1 =
      json::parse(R"({"and": [{"var": "is_admin"}, {"var": "is_active"}]})");
  EXPECT_EQ(json_logic_.Apply(logic1, data), true);

  // One false
  json data2 = {{"is_admin", true}, {"is_active", false}};
  EXPECT_EQ(json_logic_.Apply(logic1, data2), false);

  // With non-boolean values (truthy check)
  json logic2 =
      json::parse(R"({"and": [{"var": "is_admin"}, {"var": "age"}]})");
  EXPECT_EQ(json_logic_.Apply(logic2, data), age);
}
