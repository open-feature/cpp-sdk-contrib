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

TEST_F(EvaluatorTest, ResolveBoolean_Success) {
  nlohmann::json flags = {{"flags",
                           {{"my-bool-flag",
                             {{"state", "ENABLED"},
                              {"variants", {{"on", true}, {"off", false}}},
                              {"defaultVariant", "on"}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  auto result = evaluator_->ResolveBoolean("my-bool-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), true);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kStatic);
  EXPECT_EQ(result->GetVariant(), "on");
  EXPECT_FALSE(result->GetErrorCode().has_value());
}

TEST_F(EvaluatorTest, ResolveBoolean_FlagNotFound) {
  nlohmann::json flags = {{"flags", {}}};
  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  auto result = evaluator_->ResolveBoolean("missing-flag", true, ctx);

  EXPECT_EQ(result->GetValue(), true);  // Default value
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kFlagNotFound);
}

TEST_F(EvaluatorTest, ResolveBoolean_TypeMismatch) {
  nlohmann::json flags = {
      {"flags",
       {{"my-string-flag",
         {{"state", "ENABLED"},
          {"variants", {{"on", "string-value"}, {"off", "other-value"}}},
          {"defaultVariant", "on"}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  auto result = evaluator_->ResolveBoolean("my-string-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), false);  // Default value
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kTypeMismatch);
}

TEST_F(EvaluatorTest, ResolveBoolean_VariantNotFound) {
  nlohmann::json flags = {{"flags",
                           {{"my-broken-flag",
                             {{"state", "ENABLED"},
                              {"variants", {{"on", true}, {"off", false}}},
                              {"defaultVariant", "missing-variant"}}}}}};

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  auto result = evaluator_->ResolveBoolean("my-broken-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), false);  // Default value
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kGeneral);
  EXPECT_EQ(result->GetErrorMessage(),
            "flag: my-broken-flag doesn't contain evaluated variant: "
            "missing-variant.");
}
