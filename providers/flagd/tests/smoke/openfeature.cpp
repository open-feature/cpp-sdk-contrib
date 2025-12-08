#include <memory>
#include <optional>

#include "flagd/configuration.h"
#include "flagd/provider.h"
#include "gtest/gtest.h"
#include "openfeature/provider.h"

TEST(FlagdProviderTest, ProviderCreation) {
  flagd::FlagdProviderConfig config =
      flagd::FlagdProviderConfig().set_host("localhost");

  std::shared_ptr<openfeature::FeatureProvider> provider =
      std::make_shared<flagd::FlagdProvider>(config);

  provider->Init({}).IgnoreError();

  std::unique_ptr<openfeature::BoolResolutionDetails> details =
      provider->GetBooleanEvaluation("test_flag", false, {});

  auto expected_details = openfeature::BoolResolutionDetails{
      false, openfeature::Reason::kDefault, "default-variant", {}};

  EXPECT_EQ(details->GetValue(), expected_details.GetValue());
  EXPECT_EQ(details->GetReason(), expected_details.GetReason());
  EXPECT_EQ(details->GetVariant(), expected_details.GetVariant());
  EXPECT_EQ(details->GetErrorCode(), expected_details.GetErrorCode());
  EXPECT_EQ(details->GetErrorMessage(), expected_details.GetErrorMessage());
}
