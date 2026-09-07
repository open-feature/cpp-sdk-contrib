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
    json_logic_.RegisterOperation("fractional", flagd::Fractional);
  }

  JsonLogic json_logic_;

  json MakeBasicFlagLogic() {
    return json::parse(R"({
      "fractional": [
        { "var": "hashing_input" },
        [ "bucket1", 4 ],
        [ "bucket2", 4 ],
        [ "bucket3", 4 ],
        [ "bucket4", 4 ],
        [ "bucket5", 4 ],
        [ "bucket6", 4 ],
        [ "bucket7", 4 ],
        [ "bucket8", 4 ],
        [ "bucket9", 4 ],
        [ "bucket10", 4 ],
        [ "bucket11", 4 ],
        [ "bucket12", 4 ],
        [ "bucket13", 4 ],
        [ "bucket14", 4 ],
        [ "bucket15", 4 ],
        [ "bucket16", 4 ],
        [ "bucket17", 4 ],
        [ "bucket18", 4 ],
        [ "bucket19", 4 ],
        [ "bucket20", 4 ],
        [ "bucket21", 4 ],
        [ "bucket22", 4 ],
        [ "bucket23", 4 ],
        [ "bucket24", 4 ],
        [ "bucket25", 4 ]
      ]
    })");
  }
};

TEST_F(FlagdOpsTest, FractionalV3BasicDistribution) {
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

  // V3 Expected Values from Gherkin suite
  data["user"]["name"] = "jack";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "diamonds");

  data["user"]["name"] = "queen";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "diamonds");

  data["user"]["name"] = "ten";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "clubs");

  data["user"]["name"] = "nine";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "clubs");

  data["user"]["name"] = "3";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "spades");
}

TEST_F(FlagdOpsTest, FractionalV3Shorthand) {
  json data = json::object();
  data["$flagd"] = json::object();
  data["$flagd"]["flagKey"] = "fractional-flag-shorthand";

  json logic = json::parse(R"({
    "fractional": [
      [ "heads" ],
      [ "tails", 1 ]
    ]
  })");

  // V3 Expected Values from Gherkin suite
  data["targetingKey"] = "jon@company.com";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "tails");

  data["targetingKey"] = "jane@company.com";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "tails");

  data["targetingKey"] = "user-1";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "heads");

  data["targetingKey"] = "user-2";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "tails");

  data["targetingKey"] = "user-3";
  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "heads");
}

TEST_F(FlagdOpsTest, FractionalV3SingleEntry) {
  json data = json::object();
  data["$flagd"] = json::object();
  data["$flagd"]["flagKey"] = "fractional-single-entry-flag";
  data["targetingKey"] = "some-targeting-key";

  json logic = json::parse(R"({
    "fractional": [
      [ "single", 1 ]
    ]
  })");

  EXPECT_EQ(json_logic_.Apply(logic, data).value(), "single");
}

TEST_F(FlagdOpsTest, FractionalV3SharedSeed) {
  json data = json::object();

  json logic_a = json::parse(R"({
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

  // V3 Expected Values for flag A
  data["user"]["name"] = "seven";
  EXPECT_EQ(json_logic_.Apply(logic_a, data).value(), "hearts");

  data["user"]["name"] = "eight";
  EXPECT_EQ(json_logic_.Apply(logic_a, data).value(), "hearts");

  data["user"]["name"] = "nine";
  EXPECT_EQ(json_logic_.Apply(logic_a, data).value(), "diamonds");

  data["user"]["name"] = "two";
  EXPECT_EQ(json_logic_.Apply(logic_a, data).value(), "diamonds");

  // Flag B shared seed (ace-of-* variants)
  json logic_b = json::parse(R"({
    "fractional": [
      { "cat": [
        "shared-seed",
        { "var": "user.name" }
      ]},
      [ "ace-of-clubs", 25 ],
      [ "ace-of-diamonds", 25 ],
      [ "ace-of-hearts", 25 ],
      [ "ace-of-spades", 25 ]
    ]
  })");

  data["user"]["name"] = "seven";
  EXPECT_EQ(json_logic_.Apply(logic_b, data).value(), "ace-of-hearts");

  data["user"]["name"] = "eight";
  EXPECT_EQ(json_logic_.Apply(logic_b, data).value(), "ace-of-hearts");

  data["user"]["name"] = "nine";
  EXPECT_EQ(json_logic_.Apply(logic_b, data).value(), "ace-of-diamonds");

  data["user"]["name"] = "two";
  EXPECT_EQ(json_logic_.Apply(logic_b, data).value(), "ace-of-diamonds");
}

TEST_F(FlagdOpsTest, FractionalV3BasicTypes) {
  json basic_logic = MakeBasicFlagLogic();
  auto eval = [&](const json& val) {
    json data = json::object();
    data["hashing_input"] = val;
    auto res = json_logic_.Apply(basic_logic, data);
    EXPECT_TRUE(res.ok()) << res.status().message();
    return res.value();
  };

  EXPECT_EQ(eval(true), "bucket5");
  EXPECT_EQ(eval(false), "bucket1");
  EXPECT_EQ(eval("user1"), "bucket22");
  EXPECT_EQ(eval("user2"), "bucket16");
  EXPECT_EQ(eval(123), "bucket20");
  EXPECT_EQ(eval(456), "bucket12");
  EXPECT_EQ(eval(1.23), "bucket14");
  EXPECT_EQ(eval(4.56), "bucket18");
  EXPECT_EQ(eval("123"), "bucket22");
  EXPECT_EQ(eval("true"), "bucket24");
  EXPECT_EQ(eval("false"), "bucket1");
  EXPECT_EQ(eval("null"), "bucket8");
  EXPECT_EQ(eval("1.23"), "bucket4");
  EXPECT_EQ(eval(0), "bucket8");
}

TEST_F(FlagdOpsTest, FractionalV3FloatMapping) {
  json basic_logic = MakeBasicFlagLogic();
  auto eval = [&](const json& val) {
    json data = json::object();
    data["hashing_input"] = val;
    auto res = json_logic_.Apply(basic_logic, data);
    EXPECT_TRUE(res.ok()) << res.status().message();
    return res.value();
  };

  EXPECT_EQ(eval(1.0), "bucket23");
  EXPECT_EQ(eval(1), "bucket23");
  EXPECT_EQ(eval(1.0000000000000001), "bucket23");
  EXPECT_EQ(eval(-2.0), "bucket12");
  EXPECT_EQ(eval(-2), "bucket12");
  EXPECT_EQ(eval(9007199254740992.0), "bucket10");
  EXPECT_EQ(eval(int64_t{9007199254740992LL}), "bucket10");
}

TEST_F(FlagdOpsTest, FractionalV3ZeroValues) {
  json basic_logic = MakeBasicFlagLogic();
  auto eval = [&](const json& val) {
    json data = json::object();
    data["hashing_input"] = val;
    auto res = json_logic_.Apply(basic_logic, data);
    EXPECT_TRUE(res.ok()) << res.status().message();
    return res.value();
  };

  EXPECT_EQ(eval(0.0), "bucket8");
  EXPECT_EQ(eval(-0.0), "bucket8");
  EXPECT_EQ(eval(0), "bucket8");
}

TEST_F(FlagdOpsTest, FractionalV3IntegerLimits) {
  json basic_logic = MakeBasicFlagLogic();
  auto eval = [&](const json& val) {
    json data = json::object();
    data["hashing_input"] = val;
    auto res = json_logic_.Apply(basic_logic, data);
    EXPECT_TRUE(res.ok()) << res.status().message();
    return res.value();
  };

  EXPECT_EQ(eval(int64_t{2147483647LL}), "bucket10");
  EXPECT_EQ(eval(int64_t{2147483648LL}), "bucket25");
  EXPECT_EQ(eval(int64_t{-2147483648LL}), "bucket8");
  EXPECT_EQ(eval(int64_t{-2147483649LL}), "bucket18");
  EXPECT_EQ(eval(int64_t{9007199254740991LL}), "bucket2");
  EXPECT_EQ(eval(23), "bucket21");
  EXPECT_EQ(eval(24), "bucket11");
  EXPECT_EQ(eval(255), "bucket13");
  EXPECT_EQ(eval(256), "bucket13");
  EXPECT_EQ(eval(65535), "bucket4");
  EXPECT_EQ(eval(65536), "bucket11");
  EXPECT_EQ(eval(int64_t{4294967295LL}), "bucket14");
  EXPECT_EQ(eval(int64_t{4294967296LL}), "bucket19");
  EXPECT_EQ(eval(-24), "bucket4");
  EXPECT_EQ(eval(-25), "bucket16");
}

TEST_F(FlagdOpsTest, FractionalV3FloatLimits) {
  json basic_logic = MakeBasicFlagLogic();
  auto eval = [&](const json& val) {
    json data = json::object();
    data["hashing_input"] = val;
    auto res = json_logic_.Apply(basic_logic, data);
    EXPECT_TRUE(res.ok()) << res.status().message();
    return res.value();
  };

  EXPECT_EQ(eval(1e20), "bucket19");
  EXPECT_EQ(eval(-1e20), "bucket25");
  EXPECT_EQ(eval(65504.0), "bucket16");
  EXPECT_EQ(eval(-65504.0), "bucket9");
  EXPECT_EQ(eval(0.00006103515625), "bucket22");
  EXPECT_EQ(eval(0.000000059604644775390625), "bucket23");
  EXPECT_EQ(eval(65505.0), "bucket17");
  EXPECT_EQ(eval(-65505.0), "bucket17");
  EXPECT_EQ(eval(3.4028234663852886e+38), "bucket10");
  EXPECT_EQ(eval(-3.4028234663852886e+38), "bucket3");
  EXPECT_EQ(eval(1.1754943508222875e-38), "bucket24");
  EXPECT_EQ(eval(3.5e+38), "bucket1");
  EXPECT_EQ(eval(-3.5e+38), "bucket6");
  EXPECT_EQ(eval(1.7976931348623157e+308), "bucket17");
  EXPECT_EQ(eval(-1.7976931348623157e+308), "bucket9");
  EXPECT_EQ(eval(2.2250738585072014e-308), "bucket11");
  EXPECT_EQ(eval(4.9406564584124654e-324), "bucket2");
}

TEST_F(FlagdOpsTest, FractionalV3MapKeyOrdering) {
  json basic_logic = MakeBasicFlagLogic();
  auto eval = [&](const json& val) {
    json data = json::object();
    data["hashing_input"] = val;
    auto res = json_logic_.Apply(basic_logic, data);
    EXPECT_TRUE(res.ok()) << res.status().message();
    return res.value();
  };

  EXPECT_EQ(eval(json::parse(R"({"a": 1, "b": 2})")), "bucket23");
  EXPECT_EQ(eval(json::parse(R"({"b": 2, "a": 1})")), "bucket23");
  EXPECT_EQ(eval(json::parse(R"({"a": 1, "b": 2, "c": 3})")), "bucket18");
  EXPECT_EQ(eval(json::parse(R"({"c": 3, "a": 1, "b": 2})")), "bucket18");
  EXPECT_EQ(eval(json::parse(R"({"z": 1, "aa": 2})")), "bucket21");
  EXPECT_EQ(eval(json::parse(R"({"aa": 2, "z": 1})")), "bucket21");
  EXPECT_EQ(eval(json::parse(R"({"a": {"b": 1}, "b": 2})")), "bucket9");
  EXPECT_EQ(eval(json::parse(R"({"b": 2, "a": {"b": 1}})")), "bucket9");
  EXPECT_EQ(
      eval(json::parse(R"({"a": true, "bbb": 1, "c": "text", "dddd": 2.5})")),
      "bucket25");
  EXPECT_EQ(
      eval(json::parse(R"({"c": "text", "bbb": 1, "a": true, "dddd": 2.5})")),
      "bucket25");
  EXPECT_EQ(eval(json::parse(R"({"aaa": 1, "ÿ": 2})")), "bucket12");
  EXPECT_EQ(eval(json::parse(R"({"ÿ": 2, "aaa": 1})")), "bucket12");
  EXPECT_EQ(eval(json::parse(R"({"": 2, "abc": 1})")), "bucket25");
  EXPECT_EQ(eval(json::parse(R"({"key": -0.0})")), "bucket10");
  EXPECT_EQ(eval(json::parse(R"({"key": 0})")), "bucket10");
  EXPECT_EQ(eval(json::parse(R"({"key": 0.0})")), "bucket10");
  EXPECT_EQ(eval(json::parse(R"({"b": -0.0, "a": {"b": 1}, "c": 0.0})")),
            "bucket8");
  EXPECT_EQ(eval(json::parse(R"({"c": 0.0, "b": -0.0, "a": {"b": 1}})")),
            "bucket8");
  EXPECT_EQ(
      eval(json::parse(R"({"café": "façade", "b": {"résumé": {"ÿ": true}}})")),
      "bucket22");
}

TEST_F(FlagdOpsTest, FractionalV3AdvancedStructures) {
  json basic_logic = MakeBasicFlagLogic();
  auto eval = [&](const json& val) {
    json data = json::object();
    data["hashing_input"] = val;
    auto res = json_logic_.Apply(basic_logic, data);
    EXPECT_TRUE(res.ok()) << res.status().message();
    return res.value();
  };

  EXPECT_EQ(eval(json::parse(R"({})")), "bucket3");
  EXPECT_EQ(eval(json::parse(R"({"a": {}})")), "bucket20");
  EXPECT_EQ(eval(json::parse(R"({"b": 2, "a": {}})")), "bucket20");
  EXPECT_EQ(eval(json::parse(R"({"a": {}, "b": {}})")), "bucket21");
  EXPECT_EQ(eval(json::parse(R"({"a": {}, "b": []})")), "bucket7");
  EXPECT_EQ(eval(json::parse(R"({"c": 0.0, "a": []})")), "bucket18");
  EXPECT_EQ(eval(json::parse(R"({"c": 0.0, "b": -0.0, "a": {"b": []}})")),
            "bucket9");
  EXPECT_EQ(eval(json::parse(R"({"a": []})")), "bucket12");
  EXPECT_EQ(eval(json::parse(R"({"a": [1, [], 3], "b": null})")), "bucket3");
  EXPECT_EQ(eval(json::parse(R"({"a": ["a", "b", {}], "b": [1, null, 2]})")),
            "bucket23");
  EXPECT_EQ(eval(json::parse(R"({"a": [1, "two", true, [], null]})")),
            "bucket23");
  EXPECT_EQ(eval(json::parse(R"({"a": [false, 2.5, ""], "b": [], "c": null})")),
            "bucket15");
  EXPECT_EQ(eval(json::parse(R"({"x": [{"a": 1}, {"b": 2}, {"b": null}]})")),
            "bucket3");
  EXPECT_EQ(eval(json::parse(R"({"x": [{"a": {"b": []}}]})")), "bucket16");
  EXPECT_EQ(eval(json::parse(R"({"x": [{"a": {"b": null}}]})")), "bucket6");
  EXPECT_EQ(eval(json::parse(R"({"x": [{"a": {"b": {}}}]})")), "bucket2");
  EXPECT_EQ(eval(json::parse(R"({"": null})")), "bucket25");
  EXPECT_EQ(eval(json::parse(R"({"": {"": {"": ""}}})")), "bucket11");
  EXPECT_EQ(eval(json::parse(R"({"a": [[[[[]]]]]})")), "bucket15");
  EXPECT_EQ(eval(json::parse(R"({"a": [[[[null]]]]})")), "bucket7");
  EXPECT_EQ(eval(json::parse(R"({"a": {"b": {"c": {"d": {}}}}})")), "bucket21");
  EXPECT_EQ(eval(json::parse(R"({"a": {"b": {"c": {"d": null}}}})")),
            "bucket19");
}

TEST_F(FlagdOpsTest, FractionalV3StringLengthBoundaries) {
  json basic_logic = MakeBasicFlagLogic();
  auto eval = [&](const json& val) {
    json data = json::object();
    data["hashing_input"] = val;
    auto res = json_logic_.Apply(basic_logic, data);
    EXPECT_TRUE(res.ok()) << res.status().message();
    return res.value();
  };

  EXPECT_EQ(eval(""), "bucket4");
  EXPECT_EQ(eval("a"), "bucket6");
  EXPECT_EQ(eval("12345678901234567890123"), "bucket4");
  EXPECT_EQ(eval("123456789012345678901234"), "bucket14");
  EXPECT_EQ(eval(std::string(255, 'a')), "bucket18");
  EXPECT_EQ(eval(std::string(256, 'a')), "bucket7");
}

TEST_F(FlagdOpsTest, FractionalV3Utf8Strings) {
  json basic_logic = MakeBasicFlagLogic();
  auto eval = [&](const json& val) {
    json data = json::object();
    data["hashing_input"] = val;
    auto res = json_logic_.Apply(basic_logic, data);
    EXPECT_TRUE(res.ok()) << res.status().message();
    return res.value();
  };

  EXPECT_EQ(eval("あいうえおかき"), "bucket17");
  EXPECT_EQ(eval("あいうえおかきく"), "bucket16");
  EXPECT_EQ(eval("ééééééééééé"), "bucket16");
  EXPECT_EQ(eval("éééééééééééé"), "bucket19");
  EXPECT_EQ(eval("café façade résumé café"), "bucket9");
  EXPECT_EQ(eval("A\\u0000B"), "bucket15");
  EXPECT_EQ(eval("e\\u0301"), "bucket7");
  EXPECT_EQ(eval("\\uD83D\\uDE00"), "bucket23");
}

TEST_F(FlagdOpsTest, FractionalV3ImplicitTargetingKeyValidation) {
  json logic = json::parse(R"({
    "fractional": [
      [ "heads" ],
      [ "tails", 1 ]
    ]
  })");

  // Missing targetingKey returns error
  json data_missing = json::object();
  data_missing["$flagd"] = json::object();
  data_missing["$flagd"]["flagKey"] = "shorthand-flag";
  EXPECT_FALSE(json_logic_.Apply(logic, data_missing).ok());

  // Integer targetingKey returns error
  json data_int = data_missing;
  data_int["targetingKey"] = 12345;
  EXPECT_FALSE(json_logic_.Apply(logic, data_int).ok());

  // Boolean targetingKey returns error
  json data_bool = data_missing;
  data_bool["targetingKey"] = true;
  EXPECT_FALSE(json_logic_.Apply(logic, data_bool).ok());
}

TEST_F(FlagdOpsTest, FractionalV3NullInputRejection) {
  // First element evaluating to null (e.g. missing variable)
  json logic_missing_var = json::parse(R"({
    "fractional": [
      { "var": "missing_key" },
      [ "one", 50 ],
      [ "two", 50 ]
    ]
  })");
  EXPECT_FALSE(json_logic_.Apply(logic_missing_var, json::object()).ok());

  // First element literal null
  json logic_null = json::parse(R"({
    "fractional": [
      null,
      [ "one", 50 ],
      [ "two", 50 ]
    ]
  })");
  EXPECT_FALSE(json_logic_.Apply(logic_null, json::object()).ok());
}

TEST_F(FlagdOpsTest, FractionalV3WeightsEdgeCases) {
  // All-zero bucket weights must return error
  json logic_zeros = json::parse(R"({
    "fractional": [
      { "var": "targetingKey" },
      [ "one", 0 ],
      [ "two", 0 ]
    ]
  })");
  json data = {{"targetingKey", "any-user"}};
  EXPECT_FALSE(json_logic_.Apply(logic_zeros, data).ok());

  // Negative bucket weight is clamped to zero: ["one", -50] -> ["one", 0]
  json logic_neg = json::parse(R"({
    "fractional": [
      { "var": "targetingKey" },
      [ "one", -50 ],
      [ "two", 100 ]
    ]
  })");
  EXPECT_EQ(json_logic_.Apply(logic_neg, data).value(), "two");
}

TEST_F(FlagdOpsTest, FractionalV3AsCondition) {
  // Fractional as condition evaluating truthy path
  json logic_true = json::parse(R"({
    "if": [
      {
        "fractional": [
          [ false, 0 ],
          [ true, 100 ]
        ]
      },
      "big",
      "small"
    ]
  })");
  json data = json::object();
  data["$flagd"] = {{"flagKey", "fractional-as-condition-flag"}};
  data["targetingKey"] = "some-targeting-key";
  EXPECT_EQ(json_logic_.Apply(logic_true, data).value(), "big");

  // Fractional as condition evaluating false path
  json logic_false = json::parse(R"({
    "if": [
      {
        "fractional": [
          [ false, 100 ],
          [ true, 0 ]
        ]
      },
      "big",
      "small"
    ]
  })");
  data["$flagd"] = {{"flagKey", "fractional-as-condition-false-flag"}};
  EXPECT_EQ(json_logic_.Apply(logic_false, data).value(), "small");
}

TEST_F(FlagdOpsTest, FractionalV3NestedWeightLogic) {
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
}
