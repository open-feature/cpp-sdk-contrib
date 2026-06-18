#include "providers/flagd/src/evaluator/flagd_ops.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "providers/flagd/src/evaluator/json_logic/json_logic.h"

using json_logic::JsonLogic;
using nlohmann::json;

class FlagdOpsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    json_logic_.RegisterOperation("starts_with", flagd::StartsWith);
    json_logic_.RegisterOperation("ends_with", flagd::EndsWith);
    json_logic_.RegisterOperation("sem_ver", flagd::SemVer);
  }

  JsonLogic json_logic_;
};

TEST_F(FlagdOpsTest, StartsWithBasic) {
  json data = json::object();

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
}

TEST_F(FlagdOpsTest, StartsWithArgEvaluation) {
  json data = json::object();

  data["prefix"] = "hello";
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(
                     R"({"starts_with": ["hello world", {"var": "prefix"}]})"),
                 data)
          .value());
}

TEST_F(FlagdOpsTest, StartsWithNegative) {
  json data = json::object();

  EXPECT_FALSE(
      json_logic_.Apply(json::parse(R"({"starts_with": ["a", "abc"]})"), data)
          .value());
  EXPECT_FALSE(
      json_logic_.Apply(json::parse(R"({"starts_with": ["", "a"]})"), data)
          .value());
}

TEST_F(FlagdOpsTest, EndsWithBasic) {
  json data = json::object();

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
}

TEST_F(FlagdOpsTest, EndsWithArgEvaluation) {
  json data = json::object();

  data["suffix"] = "world";
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(
                     R"({"ends_with": ["hello world", {"var": "suffix"}]})"),
                 data)
          .value());
}

TEST_F(FlagdOpsTest, EndsWithNegative) {
  json data = json::object();

  EXPECT_FALSE(
      json_logic_.Apply(json::parse(R"({"ends_with": ["a", "abc"]})"), data)
          .value());
  EXPECT_FALSE(
      json_logic_.Apply(json::parse(R"({"ends_with": ["", "a"]})"), data)
          .value());
}

TEST_F(FlagdOpsTest, SemVerBasic) {
  json data = json::object();

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
}

TEST_F(FlagdOpsTest, SemVerPreRelease) {
  json data = json::object();

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
}

TEST_F(FlagdOpsTest, SemVerBuildMetadata) {
  json data = json::object();

  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(
                     R"({"sem_ver": ["1.0.0+build.1", "=", "1.0.0+build.2"]})"),
                 data)
          .value());
}

TEST_F(FlagdOpsTest, SemVerPrefix) {
  json data = json::object();

  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["v1.2.3", "=", "1.2.3"]})"), data)
          .value());
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["1.2.3", "=", "V1.2.3"]})"), data)
          .value());
}

TEST_F(FlagdOpsTest, SemVerCompatibleOperators) {
  json data = json::object();

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
}

TEST_F(FlagdOpsTest, SemVerPartialVersion) {
  json data = json::object();

  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["v1.2", "=", "1.2.0"]})"), data)
          .value());
  EXPECT_TRUE(
      json_logic_
          .Apply(json::parse(R"({"sem_ver": ["v1", "=", "1.0.0"]})"), data)
          .value());
}
