#include "providers/flagd/src/evaluator/flagd_ops.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "providers/flagd/src/evaluator/json_logic/json_logic.h"

using json_logic::JsonLogic;
using nlohmann::json;

class FlagdOpsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    json_logic_.RegisterOperation("starts_with", flagd::StartsWith);
    json_logic_.RegisterOperation("ends_with", flagd::EndsWith);
    json_logic_.RegisterOperation("sem_ver", flagd::SemVer);
    json_logic_.RegisterOperation("fractional", flagd::Fractional);
  }

  JsonLogic json_logic_;
};

TEST_F(FlagdOpsTest, StartsWith) {
  json data = json::object();

  // Basic success
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"starts_with": ["hello world", "hello"]})"),
                 data)
          .value());
  EXPECT_FALSE(
      json_logic_
          .Apply(json::parse(R"({"starts_with": ["hello world", "world"]})"),
                 data)
          .value());

  // Evaluation of arguments
  data["prefix"] = "hello";
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(
                     R"({"starts_with": ["hello world", {"var": "prefix"}]})"),
                 data)
          .value());

  // Error cases
  EXPECT_FALSE(
      json_logic_.Apply(json::parse(R"({"starts_with": ["a", "abc"]})"), data)
          .value());
  EXPECT_FALSE(
      json_logic_.Apply(json::parse(R"({"starts_with": ["", "a"]})"), data)
          .value());
}

TEST_F(FlagdOpsTest, EndsWith) {
  json data = json::object();

  // Basic success
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"ends_with": ["hello world", "world"]})"),
                 data)
          .value());
  EXPECT_FALSE(
      json_logic_
          .Apply(json::parse(R"({"ends_with": ["hello world", "hello"]})"),
                 data)
          .value());

  // Evaluation of arguments
  data["suffix"] = "world";
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(
                     R"({"ends_with": ["hello world", {"var": "suffix"}]})"),
                 data)
          .value());

  // Error cases
  EXPECT_FALSE(
      json_logic_.Apply(json::parse(R"({"ends_with": ["a", "abc"]})"), data)
          .value());
  EXPECT_FALSE(
      json_logic_.Apply(json::parse(R"({"ends_with": ["", "a"]})"), data)
          .value());
}

TEST_F(FlagdOpsTest, SemVer) {
  json data = json::object();

  // Basic comparison
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["1.2.3", "=", "1.2.3"]})"), data)
          .value());
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["1.2.3", "!=", "1.2.4"]})"), data)
          .value());
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["2.0.0", ">", "1.9.9"]})"), data)
          .value());
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["1.0.0", "<", "2.0.0"]})"), data)
          .value());
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["1.2.3", ">=", "1.2.3"]})"), data)
          .value());
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["1.2.3", "<=", "1.2.3"]})"), data)
          .value());

  // Pre-release precedence
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["1.0.0-alpha", "<", "1.0.0"]})"),
                 data)
          .value());
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(
                     R"({"sem_ver": ["1.0.0-alpha", "<", "1.0.0-alpha.1"]})"),
                 data)
          .value());

  // Build metadata ignored
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(
                     R"({"sem_ver": ["1.0.0+build.1", "=", "1.0.0+build.2"]})"),
                 data)
          .value());

  // v/V prefix
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["v1.2.3", "=", "1.2.3"]})"), data)
          .value());
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["1.2.3", "=", "V1.2.3"]})"), data)
          .value());

  // Compatible operators (^ and ~)
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["1.2.3", "^", "1.0.0"]})"), data)
          .value());
  EXPECT_FALSE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["2.0.0", "^", "1.0.0"]})"), data)
          .value());
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["1.2.3", "~", "1.2.0"]})"), data)
          .value());
  EXPECT_FALSE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["1.3.0", "~", "1.2.0"]})"), data)
          .value());
  // Partial version support (v1.2 -> 1.2.0, v1 -> 1.0.0)
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["v1.2", "=", "1.2.0"]})"), data)
          .value());
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["v1", "=", "1.0.0"]})"), data)
          .value());
}
