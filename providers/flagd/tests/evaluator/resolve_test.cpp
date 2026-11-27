#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <tuple>
#include <variant>

#include "flagd/evaluator/evaluator.h"
#include "flagd/evaluator/flagd_ops.h"
#include "flagd/sync/sync.h"

namespace {

class TestableSync : public flagd::FlagSync {
 public:
  using flagd::FlagSync::FlagSync;

  absl::Status Init(const openfeature::EvaluationContext& ctx) override {
    return absl::OkStatus();
  }
  absl::Status Shutdown() override { return absl::OkStatus(); }

  absl::Status TriggerUpdate(const nlohmann::json& new_json) {
    return this->UpdateFlags(new_json);
  }
};

// variants on/off, with a defaultVariant unless default_variant is empty
nlohmann::json Flags(const nlohmann::json& targeting,
                     const std::string& default_variant = "off") {
  nlohmann::json flag = {
      {"state", "ENABLED"},
      {"variants", {{"on", "on-value"}, {"off", "off-value"}}},
      {"targeting", targeting},
      {"metadata", {{"details", "flag"}, {"owner", "flag"}}},
  };
  if (!default_variant.empty()) flag["defaultVariant"] = default_variant;
  return {
      {"flags", {{"flag", flag}}},
      {"metadata",
       {{"flagSetId", "set"},
        {"version", "1"},
        {"details", "set"},
        {"team", "set"}}},
  };
}

std::string MetadataString(const openfeature::FlagMetadata& metadata,
                           const std::string& key) {
  auto iter = metadata.data.find(key);
  if (iter == metadata.data.end()) return "<missing>";
  if (const auto* str = std::get_if<std::string>(&iter->second)) return *str;
  return "<not a string>";
}

class ResolveTest : public ::testing::Test {
 protected:
  void SetUp() override {
    sync_ = std::make_shared<TestableSync>();
    evaluator_ = std::make_unique<flagd::JsonLogicEvaluator>(sync_);
  }

  std::unique_ptr<openfeature::StringResolutionDetails> Evaluate(
      const nlohmann::json& flags,
      const openfeature::EvaluationContext& ctx =
          openfeature::EvaluationContext::Builder().Build()) {
    absl::Status status = sync_->TriggerUpdate(flags);
    EXPECT_TRUE(status.ok()) << status.message();
    return evaluator_->ResolveString("flag", "code-default", ctx);
  }

  std::shared_ptr<TestableSync> sync_;
  std::unique_ptr<flagd::JsonLogicEvaluator> evaluator_;
};

TEST_F(ResolveTest, WholeRuleSelectsVariant) {
  auto result = Evaluate(Flags(R"({"resolve": ["on"]})"_json));
  EXPECT_EQ(result->GetValue(), "on-value");
  EXPECT_EQ(result->GetVariant(), "on");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kTargetingMatch);
  EXPECT_FALSE(result->GetErrorCode().has_value());
}

TEST_F(ResolveTest, StandardReasonReplacesReason) {
  auto result = Evaluate(Flags(R"({"resolve": ["on", "SPLIT"]})"_json));
  EXPECT_EQ(result->GetValue(), "on-value");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kSplit);
}

// openfeature::Reason is a closed enum, so a custom reason can't be reported
TEST_F(ResolveTest, CustomReasonKeepsReason) {
  auto result = Evaluate(Flags(R"({"resolve": ["on", "ALLOWLIST"]})"_json));
  EXPECT_EQ(result->GetValue(), "on-value");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kTargetingMatch);
}

TEST_F(ResolveTest, NullReasonKeepsReasonAndAddsMetadata) {
  auto result = Evaluate(
      Flags(R"({"resolve": ["on", null, [["details", "rule"]]]})"_json));
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kTargetingMatch);
  EXPECT_EQ(MetadataString(result->GetFlagMetadata(), "details"), "rule");
}

TEST_F(ResolveTest, RuleMetadataWinsOverFlagAndFlagSet) {
  auto result = Evaluate(
      Flags(R"({"resolve": ["on", null, [["details", "rule"]]]})"_json));
  const auto& metadata = result->GetFlagMetadata();
  EXPECT_EQ(MetadataString(metadata, "details"), "rule");
  EXPECT_EQ(MetadataString(metadata, "owner"), "flag");
  EXPECT_EQ(MetadataString(metadata, "team"), "set");
}

TEST_F(ResolveTest, ReservedMetadataKeysAreSkipped) {
  auto result = Evaluate(Flags(R"({"resolve": ["on", null, [
    [{"cat": ["flagSet", "Id"]}, "rule"],
    [{"cat": ["vers", "ion"]}, "rule"],
    ["details", "rule"]
  ]]})"_json));
  const auto& metadata = result->GetFlagMetadata();
  EXPECT_EQ(MetadataString(metadata, "flagSetId"), "set");
  EXPECT_EQ(MetadataString(metadata, "version"), "1");
  EXPECT_EQ(MetadataString(metadata, "details"), "rule");
}

TEST_F(ResolveTest, NestedIfBranches) {
  nlohmann::json targeting = R"({"if": [
    {"==": [{"var": "id"}, "a"]}, {"resolve": ["on", "SPLIT", [["details", "a"]]]},
    {"==": [{"var": "id"}, "b"]}, {"resolve": ["on", null, [["details", "b"]]]},
    {"resolve": ["off", null, [["details", "else"]]]}
  ]})"_json;
  for (const auto& [id, variant, details] :
       {std::tuple{"a", "on", "a"}, std::tuple{"b", "on", "b"},
        std::tuple{"c", "off", "else"}}) {
    auto ctx = openfeature::EvaluationContext::Builder()
                   .WithAttribute("id", id)
                   .Build();
    auto result = Evaluate(Flags(targeting), ctx);
    EXPECT_EQ(result->GetVariant(), variant) << id;
    EXPECT_EQ(MetadataString(result->GetFlagMetadata(), "details"), details)
        << id;
  }
}

TEST_F(ResolveTest, BooleanVariantIsCoerced) {
  nlohmann::json flags = Flags(R"({"resolve": [true]})"_json);
  flags["flags"]["flag"]["variants"] = {{"true", "yes"}, {"false", "no"}};
  flags["flags"]["flag"]["defaultVariant"] = "false";
  auto result = Evaluate(flags);
  EXPECT_EQ(result->GetValue(), "yes");
  EXPECT_EQ(result->GetVariant(), "true");
}

TEST_F(ResolveTest, FractionalBucketVariant) {
  auto result = Evaluate(Flags(R"({"fractional": [{"var": "targetingKey"},
    [{"resolve": ["on", "SPLIT", [["details", "bucket"]]]}, 1],
    ["off", 0]
  ]})"_json),
                         openfeature::EvaluationContext::Builder()
                             .WithTargetingKey("user")
                             .Build());
  EXPECT_EQ(result->GetVariant(), "on");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kSplit);
  EXPECT_EQ(MetadataString(result->GetFlagMetadata(), "details"), "bucket");
}

TEST_F(ResolveTest, ShorthandFractionalBucketVariant) {
  auto ctx = openfeature::EvaluationContext::Builder()
                 .WithTargetingKey("user")
                 .Build();
  auto result = Evaluate(
      Flags(
          R"({"fractional": [[{"resolve": ["on", "SPLIT"]}, 1], ["off", 0]]})"_json),
      ctx);
  EXPECT_EQ(result->GetVariant(), "on");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kSplit);
}

TEST_F(ResolveTest, ComputedArguments) {
  auto ctx = openfeature::EvaluationContext::Builder()
                 .WithAttribute("pick", "on")
                 .WithAttribute("why", "SPLIT")
                 .WithAttribute("key", "details")
                 .Build();
  auto result = Evaluate(Flags(R"({"resolve": [
    {"var": "pick"}, {"var": "why"}, [[{"var": "key"}, {"cat": ["computed ", {"var": "pick"}]}]]
  ]})"_json),
                         ctx);
  EXPECT_EQ(result->GetVariant(), "on");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kSplit);
  EXPECT_EQ(MetadataString(result->GetFlagMetadata(), "details"),
            "computed on");
}

TEST_F(ResolveTest, MissingVariantFailsLikePlainString) {
  auto result = Evaluate(Flags(R"({"resolve": ["missing"]})"_json));
  EXPECT_EQ(result->GetValue(), "code-default");
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kGeneral);
}

TEST_F(ResolveTest, NonFiniteMetadataFallsBackToDefaultVariant) {
  auto result = Evaluate(
      Flags(R"({"resolve": ["on", null, [["n", {"/": [1, 0]}]]]})"_json));
  EXPECT_EQ(result->GetVariant(), "off");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
}

// malformed computed arguments return null, so the default variant is used
class ResolveMalformedTest
    : public ResolveTest,
      public ::testing::WithParamInterface<
          std::tuple<const char*, const char*, const char*>> {};

TEST_P(ResolveMalformedTest, FallsBackToDefaultVariant) {
  auto [variant, reason, metadata] = GetParam();
  auto ctx = openfeature::EvaluationContext::Builder()
                 .WithAttribute("v", nlohmann::json::parse(variant))
                 .WithAttribute("r", nlohmann::json::parse(reason))
                 .WithAttribute("m", nlohmann::json::parse(metadata))
                 .Build();
  auto result = Evaluate(
      Flags(R"({"resolve": [{"var": "v"}, {"var": "r"}, {"var": "m"}]})"_json),
      ctx);
  EXPECT_EQ(result->GetValue(), "off-value");
  EXPECT_EQ(result->GetVariant(), "off");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
  EXPECT_FALSE(result->GetErrorCode().has_value());
}

INSTANTIATE_TEST_SUITE_P(
    Args, ResolveMalformedTest,
    ::testing::Values(
        std::make_tuple(R"({"a": 1})", "null", "null"),  // object variant
        std::make_tuple("[1]", "null", "null"),          // array variant
        std::make_tuple(R"("on")", "1", "null"),         // non-string reason
        std::make_tuple(R"("on")", "null", R"("details")"),      // not pairs
        std::make_tuple(R"("on")", "null", R"([["details"]])"),  // short pair
        std::make_tuple(R"("on")", "null", R"([[1, "x"]])"),  // non-string key
        std::make_tuple(R"("on")", "null",
                        R"([["details", {"a": 1}]])")));  // object value

// a null variant falls back exactly like a plain null, with the rule's reason

TEST_F(ResolveTest, PlainNullReportsDefault) {
  auto result = Evaluate(Flags(R"({"if": [false, "on", null]})"_json));
  EXPECT_EQ(result->GetVariant(), "off");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
}

TEST_F(ResolveTest, NullVariantWithDefaultVariant) {
  auto result = Evaluate(Flags(R"({"resolve": [null]})"_json));
  EXPECT_EQ(result->GetValue(), "off-value");
  EXPECT_EQ(result->GetVariant(), "off");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
}

TEST_F(ResolveTest, NullVariantWithDefaultVariantAndReason) {
  auto result = Evaluate(
      Flags(R"({"resolve": [null, "DISABLED", [["details", "rule"]]]})"_json));
  EXPECT_EQ(result->GetValue(), "off-value");
  EXPECT_EQ(result->GetVariant(), "off");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDisabled);
  EXPECT_EQ(MetadataString(result->GetFlagMetadata(), "details"), "rule");
}

TEST_F(ResolveTest, NullVariantWithoutDefaultVariant) {
  auto result = Evaluate(Flags(R"({"resolve": [null]})"_json, ""));
  EXPECT_EQ(result->GetValue(), "code-default");
  EXPECT_EQ(result->GetVariant(), "");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
  EXPECT_FALSE(result->GetErrorCode().has_value());
}

TEST_F(ResolveTest, NullVariantWithoutDefaultVariantAndCustomReason) {
  auto result =
      Evaluate(Flags(R"({"resolve": [null, "OFF_BY_POLICY"]})"_json, ""));
  EXPECT_EQ(result->GetValue(), "code-default");
  EXPECT_EQ(result->GetVariant(), "");
  // custom reasons need open-feature/cpp-sdk#161
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
  EXPECT_FALSE(result->GetErrorCode().has_value());
}

TEST_F(ResolveTest, DisabledWithOverrides) {
  nlohmann::json flags = Flags(R"({"if": [
    {"in": [{"var": "targetingKey"}, ["alice"]]}, "on", {"resolve": [null, "DISABLED"]}
  ]})"_json,
                               "");
  auto alice = Evaluate(flags, openfeature::EvaluationContext::Builder()
                                   .WithTargetingKey("alice")
                                   .Build());
  EXPECT_EQ(alice->GetValue(), "on-value");
  EXPECT_EQ(alice->GetReason(), openfeature::Reason::kTargetingMatch);

  auto bob = Evaluate(flags, openfeature::EvaluationContext::Builder()
                                 .WithTargetingKey("bob")
                                 .Build());
  EXPECT_EQ(bob->GetValue(), "code-default");
  EXPECT_EQ(bob->GetVariant(), "");
  EXPECT_EQ(bob->GetReason(), openfeature::Reason::kDisabled);
  EXPECT_FALSE(bob->GetErrorCode().has_value());
}

// a reason does not change the outcome
TEST_F(ResolveTest, SelectedVariantWithDisabledReason) {
  auto result = Evaluate(Flags(R"({"resolve": ["on", "DISABLED"]})"_json));
  EXPECT_EQ(result->GetValue(), "on-value");
  EXPECT_EQ(result->GetVariant(), "on");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDisabled);
}

TEST_F(ResolveTest, ErrorReasonIsNotAFailedEvaluation) {
  auto result = Evaluate(Flags(R"({"resolve": ["on", "ERROR"]})"_json));
  EXPECT_EQ(result->GetValue(), "on-value");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_FALSE(result->GetErrorCode().has_value());
}

TEST_F(ResolveTest, FractionalNonPrimitiveVariantReturnsNull) {
  auto result = Evaluate(
      Flags(
          R"({"fractional": [{"var": "targetingKey"}, [{"var": "obj"}, 1], ["on", 0]]})"_json),
      openfeature::EvaluationContext::Builder()
          .WithTargetingKey("user")
          .WithAttribute("obj", nlohmann::json{{"a", 1}})
          .Build());
  EXPECT_EQ(result->GetVariant(), "off");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
}

// context data can't invoke resolve, or be taken for its result

TEST_F(ResolveTest, ContextCannotInvokeResolve) {
  auto ctx =
      openfeature::EvaluationContext::Builder()
          .WithAttribute("pick", R"({"resolve": ["on", "DISABLED"]})"_json)
          .Build();
  auto result =
      Evaluate(Flags(R"({"if": [true, {"var": "pick"}, "on"]})"_json), ctx);
  EXPECT_EQ(result->GetVariant(), "off");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
}

TEST_F(ResolveTest, ContextCannotForgeResolveResult) {
  nlohmann::json forged = nlohmann::json::binary(
      nlohmann::json::to_cbor(
          {{"variant", "on"}, {"reason", "DISABLED"}, {"metadata", {}}}),
      flagd::kResolveSubtype);
  // e.g. decoded from a user's MessagePack payload, which keeps the subtype
  nlohmann::json decoded = nlohmann::json::from_msgpack(
      nlohmann::json::to_msgpack({{"pick", forged}}));
  ASSERT_TRUE(flagd::IsResolveResult(decoded["pick"]));

  auto ctx = openfeature::EvaluationContext::Builder()
                 .WithAttribute("pick", decoded["pick"])
                 .Build();
  auto result =
      Evaluate(Flags(R"({"if": [true, {"var": "pick"}, "on"]})"_json), ctx);
  EXPECT_EQ(result->GetVariant(), "off");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
}

}  // namespace
