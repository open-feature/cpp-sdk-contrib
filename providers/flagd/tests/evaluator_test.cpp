#include "flagd/evaluator.h"

#include <gtest/gtest.h>

#include <memory>
#include <nlohmann/json.hpp>

#include "flagd/sync.h"

class TestableSync : public flagd::FlagSync {
 public:
  using flagd::FlagSync::FlagSync;

  absl::Status Init(const openfeature::EvaluationContext& ctx) override {
    return absl::OkStatus();
  }
  absl::Status Shutdown() override { return absl::OkStatus(); }

  void TriggerUpdate(const nlohmann::json& new_json) {
    this->UpdateFlags(new_json);
  }
};

class EvaluatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    sync_ = std::make_shared<TestableSync>();
    evaluator_ = std::make_unique<flagd::JsonLogicEvaluator>(sync_);
  }

  std::shared_ptr<TestableSync> sync_;
  std::unique_ptr<flagd::JsonLogicEvaluator> evaluator_;
};

TEST_F(EvaluatorTest, ResolveBooleanSuccess) {
  nlohmann::json flags = {{"flags",
                           {{"my-bool-flag",
                             {{"state", "ENABLED"},
                              {"variants", {{"on", true}, {"off", false}}},
                              {"defaultVariant", "on"}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("my-bool-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), true);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kStatic);
  EXPECT_EQ(result->GetVariant(), "on");
  EXPECT_FALSE(result->GetErrorCode().has_value());
}

TEST_F(EvaluatorTest, ResolveBooleanFlagNotFound) {
  nlohmann::json flags = {{"flags", nlohmann::json::object()}};
  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("missing-flag", true, ctx);

  EXPECT_EQ(result->GetValue(), true);  // Default value
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kFlagNotFound);
}

TEST_F(EvaluatorTest, ResolveBooleanTypeMismatch) {
  nlohmann::json flags = {
      {"flags",
       {{"my-string-flag",
         {{"state", "ENABLED"},
          {"variants", {{"on", "string-value"}, {"off", "other-value"}}},
          {"defaultVariant", "on"}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("my-string-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), false);  // Default value
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kTypeMismatch);
}

TEST_F(EvaluatorTest, ResolveBooleanVariantNotFound) {
  nlohmann::json flags = {{"flags",
                           {{"my-broken-flag",
                             {{"state", "ENABLED"},
                              {"variants", {{"on", true}, {"off", false}}},
                              {"defaultVariant", "missing-variant"}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("my-broken-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), false);  // Default value
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kGeneral);
  EXPECT_EQ(result->GetErrorMessage(),
            "flag: my-broken-flag doesn't contain evaluated variant: "
            "missing-variant.");
}

TEST_F(EvaluatorTest, ResolveStringSuccess) {
  nlohmann::json flags = {
      {"flags",
       {{"my-string-flag",
         {{"state", "ENABLED"},
          {"variants", {{"v1", "value1"}, {"v2", "value2"}}},
          {"defaultVariant", "v1"}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::StringResolutionDetails> result =
      evaluator_->ResolveString("my-string-flag", "default", ctx);

  EXPECT_EQ(result->GetValue(), "value1");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kStatic);
  EXPECT_EQ(result->GetVariant(), "v1");
}

TEST_F(EvaluatorTest, ResolveIntegerSuccess) {
  nlohmann::json flags = {{"flags",
                           {{"my-int-flag",
                             {{"state", "ENABLED"},
                              {"variants", {{"one", 1}, {"two", 2}}},
                              {"defaultVariant", "two"}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::IntResolutionDetails> result =
      evaluator_->ResolveInteger("my-int-flag", 0, ctx);

  EXPECT_EQ(result->GetValue(), 2);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kStatic);
  EXPECT_EQ(result->GetVariant(), "two");
}

TEST_F(EvaluatorTest, ResolveDoubleSuccess) {
  float d1 = 1.1, d2 = 2.2;
  nlohmann::json flags = {{"flags",
                           {{"my-double-flag",
                             {{"state", "ENABLED"},
                              {"variants", {{"d1", d1}, {"d2", d2}}},
                              {"defaultVariant", "d1"}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::DoubleResolutionDetails> result =
      evaluator_->ResolveDouble("my-double-flag", 0.0, ctx);

  EXPECT_DOUBLE_EQ(result->GetValue(), d1);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kStatic);
  EXPECT_EQ(result->GetVariant(), "d1");
}

TEST_F(EvaluatorTest, ResolveObjectSuccess) {
  nlohmann::json flags = {{"flags",
                           {{"my-object-flag",
                             {{"state", "ENABLED"},
                              {"variants",
                               {{"obj1", {{"foo", "bar"}, {"baz", 123}}},
                                {"obj2", {{"key", true}}}}},
                              {"defaultVariant", "obj1"}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::ObjectResolutionDetails> result =
      evaluator_->ResolveObject("my-object-flag", openfeature::Value(), ctx);

  openfeature::Value resolved_value = result->GetValue();

  EXPECT_TRUE(resolved_value.IsStructure());

  const std::map<std::string, openfeature::Value>* structure =
      resolved_value.AsStructure();
  ASSERT_NE(structure, nullptr);

  EXPECT_EQ(result->GetReason(), openfeature::Reason::kStatic);
  EXPECT_EQ(result->GetVariant(), "obj1");

  EXPECT_TRUE(structure->at("foo").IsString());
  EXPECT_EQ(structure->at("foo").AsString().value(), "bar");
  EXPECT_EQ(structure->at("baz").AsInt().value(), 123);
}

TEST_F(EvaluatorTest, ResolveStringTypeMismatch) {
  nlohmann::json flags = {{"flags",
                           {{"my-int-flag",
                             {{"state", "ENABLED"},
                              {"variants", {{"one", 1}}},
                              {"defaultVariant", "one"}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::StringResolutionDetails> result =
      evaluator_->ResolveString("my-int-flag", "default", ctx);

  EXPECT_EQ(result->GetValue(), "default");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kTypeMismatch);
}

TEST_F(EvaluatorTest, ResolveIntegerTypeMismatch) {
  nlohmann::json flags = {{"flags",
                           {{"my-string-flag",
                             {{"state", "ENABLED"},
                              {"variants", {{"v1", "val1"}}},
                              {"defaultVariant", "v1"}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::IntResolutionDetails> result =
      evaluator_->ResolveInteger("my-string-flag", 0, ctx);

  EXPECT_EQ(result->GetValue(), 0);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kTypeMismatch);
}

TEST_F(EvaluatorTest, ResolveDoubleTypeMismatch) {
  nlohmann::json flags = {{"flags",
                           {{"my-string-flag",
                             {{"state", "ENABLED"},
                              {"variants", {{"v1", "val1"}}},
                              {"defaultVariant", "v1"}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::DoubleResolutionDetails> result =
      evaluator_->ResolveDouble("my-string-flag", 0.0, ctx);

  EXPECT_EQ(result->GetValue(), 0.0);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kTypeMismatch);
}

TEST_F(EvaluatorTest, ResolveBooleanMissingDefaultVariant) {
  nlohmann::json flags = {{"flags",
                           {{"my-bool-flag",
                             {{"state", "ENABLED"},
                              {"variants", {{"on", true}, {"off", false}}}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("my-bool-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), false);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
  EXPECT_FALSE(result->GetErrorCode().has_value());
}

TEST_F(EvaluatorTest, ResolveBooleanNullDefaultVariant) {
  nlohmann::json flags = {{"flags",
                           {{"my-bool-flag",
                             {{"state", "ENABLED"},
                              {"variants", {{"on", true}, {"off", false}}},
                              {"defaultVariant", nullptr}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("my-bool-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), false);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
  EXPECT_FALSE(result->GetErrorCode().has_value());
}

TEST_F(EvaluatorTest, ResolveBooleanTargetingNullMissingDefaultVariant) {
  nlohmann::json flags = {
      {"flags",
       {{"my-bool-flag",
         {{"state", "ENABLED"},
          {"variants", {{"on", true}, {"off", false}}},
          {"targeting", {{"if", {{false}, "on", nullptr}}}}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("my-bool-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), false);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
  EXPECT_FALSE(result->GetErrorCode().has_value());
}
