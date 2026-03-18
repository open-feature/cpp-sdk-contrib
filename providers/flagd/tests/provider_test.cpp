#include "flagd/provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "flagd/evaluator/evaluator.h"
#include "flagd/sync/sync.h"

namespace flagd {

class MockSync : public FlagSync {
 public:
  MOCK_METHOD(absl::Status, Init, (const openfeature::EvaluationContext& ctx),
              (override));
  MOCK_METHOD(absl::Status, Shutdown, (), (override));
};

class MockEvaluator : public Evaluator {
 public:
  MOCK_METHOD(std::unique_ptr<openfeature::BoolResolutionDetails>,
              ResolveBoolean,
              (std::string_view flag_key, bool default_value,
               const openfeature::EvaluationContext& ctx),
              (override));
  MOCK_METHOD(std::unique_ptr<openfeature::StringResolutionDetails>,
              ResolveString,
              (std::string_view flag_key, std::string_view default_value,
               const openfeature::EvaluationContext& ctx),
              (override));
  MOCK_METHOD(std::unique_ptr<openfeature::IntResolutionDetails>,
              ResolveInteger,
              (std::string_view flag_key, int64_t default_value,
               const openfeature::EvaluationContext& ctx),
              (override));
  MOCK_METHOD(std::unique_ptr<openfeature::DoubleResolutionDetails>,
              ResolveDouble,
              (std::string_view flag_key, double default_value,
               const openfeature::EvaluationContext& ctx),
              (override));
  MOCK_METHOD(std::unique_ptr<openfeature::ObjectResolutionDetails>,
              ResolveObject,
              (std::string_view flag_key, openfeature::Value default_value,
               const openfeature::EvaluationContext& ctx),
              (override));
};

TEST(ProviderTest, MetadataReturnsExpectedValues) {
  FlagdProvider provider;
  openfeature::Metadata metadata = provider.GetMetadata();
  EXPECT_EQ(metadata.name, "flagd");
}

TEST(ProviderTest, ReturnsNotReadyBeforeInit) {
  auto mock_sync = std::make_shared<MockSync>();
  auto mock_evaluator = std::make_unique<MockEvaluator>();

  // We don't expect any calls to the evaluator when not ready
  EXPECT_CALL(*mock_evaluator, ResolveBoolean).Times(0);

  FlagdProvider provider(mock_sync, std::move(mock_evaluator));

  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      provider.GetBooleanEvaluation(
          "some-flag", false,
          openfeature::EvaluationContext::Builder().build());

  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kProviderNotReady);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
}

TEST(ProviderTest, ReturnsReadyAfterInit) {
  auto mock_sync = std::make_shared<MockSync>();
  auto mock_evaluator = std::make_unique<MockEvaluator>();

  EXPECT_CALL(*mock_sync, Init).WillOnce(testing::Return(absl::OkStatus()));

  EXPECT_CALL(*mock_evaluator, ResolveBoolean)
      .WillOnce([](std::string_view flag_key, bool default_value,
                   const openfeature::EvaluationContext& ctx) {
        return std::make_unique<openfeature::BoolResolutionDetails>(
            true, openfeature::Reason::kStatic, "on",
            openfeature::FlagMetadata());
      });

  FlagdProvider provider(mock_sync, std::move(mock_evaluator));
  (void)provider.Init(openfeature::EvaluationContext::Builder().build());

  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      provider.GetBooleanEvaluation(
          "some-flag", false,
          openfeature::EvaluationContext::Builder().build());

  EXPECT_EQ(result->GetValue(), true);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kStatic);
}

TEST(ProviderTest, DelegationWorks) {
  auto mock_sync = std::make_shared<MockSync>();
  auto mock_evaluator = std::make_unique<MockEvaluator>();

  EXPECT_CALL(*mock_sync, Init).WillOnce(testing::Return(absl::OkStatus()));

  std::string expected_flag = "my-flag";
  bool expected_default = true;

  EXPECT_CALL(*mock_evaluator,
              ResolveBoolean(testing::Eq(expected_flag),
                             testing::Eq(expected_default), testing::_))
      .WillOnce([](std::string_view flag_key, bool default_value,
                   const openfeature::EvaluationContext& ctx) {
        return std::make_unique<openfeature::BoolResolutionDetails>(
            default_value, openfeature::Reason::kDefault, std::nullopt,
            openfeature::FlagMetadata());
      });

  FlagdProvider provider(mock_sync, std::move(mock_evaluator));
  (void)provider.Init(openfeature::EvaluationContext::Builder().build());

  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      provider.GetBooleanEvaluation(
          expected_flag, expected_default,
          openfeature::EvaluationContext::Builder().build());

  EXPECT_EQ(result->GetValue(), expected_default);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
}

TEST(ProviderTest, ShutdownMakesProviderNotReady) {
  auto mock_sync = std::make_shared<MockSync>();
  auto mock_evaluator = std::make_unique<MockEvaluator>();

  EXPECT_CALL(*mock_sync, Init).WillOnce(testing::Return(absl::OkStatus()));
  EXPECT_CALL(*mock_sync, Shutdown).WillOnce(testing::Return(absl::OkStatus()));

  FlagdProvider provider(mock_sync, std::move(mock_evaluator));
  (void)provider.Init(openfeature::EvaluationContext::Builder().build());
  (void)provider.Shutdown();

  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      provider.GetBooleanEvaluation(
          "some-flag", false,
          openfeature::EvaluationContext::Builder().build());

  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kProviderNotReady);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
}

}  // namespace flagd
