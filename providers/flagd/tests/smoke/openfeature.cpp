#include <memory>
#include <optional>

#include "flagd/configuration.h"
#include "flagd/provider.h"
#include "gtest/gtest.h"
#include "openfeature/provider.h"

TEST(FlagdProviderTest, ProviderCreation) {
  flagd::FlagdProviderConfig config =
      flagd::FlagdProviderConfig().SetHost("localhost");

  std::shared_ptr<openfeature::FeatureProvider> provider =
      std::make_shared<flagd::FlagdProvider>(config);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();

  std::unique_ptr<openfeature::BoolResolutionDetails> details =
      provider->GetBooleanEvaluation("test_flag", false, ctx);

  // We didn't call Init, so the provider is not ready.
  auto expected_details = openfeature::BoolResolutionDetails{
      false,
      openfeature::Reason::kError,
      "",
      {},
      openfeature::ErrorCode::kProviderNotReady,
      "Provider not ready"};

  EXPECT_EQ(details->GetValue(), expected_details.GetValue());
  EXPECT_EQ(details->GetReason(), expected_details.GetReason());
  EXPECT_EQ(details->GetVariant(), expected_details.GetVariant());
  EXPECT_EQ(details->GetErrorCode(), expected_details.GetErrorCode());
  EXPECT_EQ(details->GetErrorMessage(), expected_details.GetErrorMessage());
}
