// TODO(#91): This file contains rewritten test cases from
// open-feature/flagd-testbed/blob/main/gherkin/targeting.feature.
// They should be removed once automatic gherkin tests are introduced.
#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "providers/flagd/src/evaluator/flagd_ops.h"
#include "providers/flagd/src/evaluator/json_logic/json_logic.h"

using json_logic::JsonLogic;
using nlohmann::json;

class FlagdOpsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Assuming these are registered
    json_logic_.RegisterOperation("starts_with", flagd::StartsWith);
    json_logic_.RegisterOperation("ends_with", flagd::EndsWith);
    json_logic_.RegisterOperation("sem_ver", flagd::SemVer);
    json_logic_.RegisterOperation("fractional", flagd::Fractional);
  }

  JsonLogic json_logic_;
};

TEST_F(FlagdOpsTest, FractionalV2BasicDistribution) {
  json data = json::object();
  data["$flagd"] = json::object();
  data["$flagd"]["flagKey"] = "fractional-flag";

  json logic = json::parse(R"({
    "fractional": [
      {"cat": [
        { "var": "$flagd.flagKey" },
        { "var": "user.name" }
      ]},
      [ "clubs", 25 ],
      [ "diamonds", 25 ],
      [ "hearts", 25 ],
      [ "spades", 25 ]
    ]
  })");

  // V2 Expected Values
  data["user"]["name"] = "jack";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "hearts");

  data["user"]["name"] = "queen";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "spades");

  data["user"]["name"] = "ten";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "clubs");

  data["user"]["name"] = "nine";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "diamonds");

  data["user"]["name"] = "3";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "clubs");
}

TEST_F(FlagdOpsTest, FractionalV2Shorthand) {
  json data = json::object();
  data["$flagd"] = json::object();
  data["$flagd"]["flagKey"] = "fractional-flag-shorthand";

  json logic = json::parse(R"({
    "fractional": [
      [ "heads" ],
      [ "tails", 1 ]
    ]
  })");

  // V2 Expected Values
  data["targetingKey"] = "jon@company.com";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "heads");

  data["targetingKey"] = "jane@company.com";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "tails");
}

TEST_F(FlagdOpsTest, FractionalV2SharedSeed) {
  json data = json::object();

  json logic = json::parse(R"({
    "fractional": [
      { "cat": [
        "shared-seed",
        { "var": "user.name" }
      ]},
      [ "clubs", 25 ],
      [ "diamonds", 25 ],
      [ "hearts", 25 ],
      [ "spades", 25 ]
    ]
  })");

  // V2 Expected Values
  data["user"]["name"] = "seven";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "hearts");

  data["user"]["name"] = "eight";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "diamonds");

  data["user"]["name"] = "nine";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "clubs");

  data["user"]["name"] = "two";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "spades");
}

TEST_F(FlagdOpsTest, FractionalV2HashEdgeCases) {
  json data = json::object();

  json logic = json::parse(R"({
    "fractional": [
      { "var": "targetingKey" },
      [ "lower", 50 ],
      [ "upper", 50 ]
    ]
  })");

  // hash = 0
  data["targetingKey"] = "ejOoVL";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "lower");

  // hash = 1
  data["targetingKey"] = "bY9fO-";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "lower");

  // hash = 2147483647 (INT32_MAX)
  data["targetingKey"] = "SI7p-";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "lower");

  // hash = 2147483648 (INT32_MIN when cast to signed 32-bit - critical
  // threshold)
  data["targetingKey"] = "6LvT0";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "upper");

  // hash = 4294967295 (UINT32_MAX / -1 when signed)
  data["targetingKey"] = "ceQdGm";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "upper");
}

TEST_F(FlagdOpsTest, FractionalV2NestedIfVariantName) {
  json data = json::object();

  json logic = json::parse(R"({
    "fractional": [
      { "var": "targetingKey" },
      [
        {
          "if": [
            { "==": [{ "var": "tier" }, "premium"] },
            "premium",
            "standard"
          ]
        },
        50
      ],
      [ "standard", 50 ]
    ]
  })");

  data["targetingKey"] = "jon@company.com";
  data["tier"] = "premium";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "premium");

  data["tier"] = "basic";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "standard");

  data["targetingKey"] = "user1";
  data["tier"] = "premium";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "standard");

  data["tier"] = "basic";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "standard");
}

TEST_F(FlagdOpsTest, FractionalV2NestedVarVariantName) {
  json data = json::object();

  json logic = json::parse(R"({
    "fractional": [
      { "var": "targetingKey" },
      [ { "var": "color" }, 50 ],
      [ "blue", 50 ]
    ]
  })");

  data["targetingKey"] = "jon@company.com";

  data["color"] = "red";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "red");

  data["color"] = "green";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "green");

  data["targetingKey"] = "user1";
  data["color"] = "red";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "blue");

  // Fallbacks handled by outer engine, json_logic returns the literal var
  // evaluations
  data["targetingKey"] = "jon@company.com";
  data["color"] = "yellow";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "yellow");

  data["color"] = "";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "");
}

TEST_F(FlagdOpsTest, FractionalV2NestedWeightLogic) {
  json data = json::object();

  json logic = json::parse(R"({
    "fractional": [
      { "var": "targetingKey" },
      [
        "red",
        {
          "if": [
            { "==": [{ "var": "tier" }, "premium"] },
            100,
            0
          ]
        }
      ],
      [ "blue", 10 ]
    ]
  })");

  data["targetingKey"] = "jon@company.com";
  data["tier"] = "premium";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "red");

  data["tier"] = "basic";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "blue");

  data["targetingKey"] = "user1";
  data["tier"] = "premium";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "red");

  data["tier"] = "basic";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "blue");
}