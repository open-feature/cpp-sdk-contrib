#ifndef CPP_SDK_FLAGD_PROVIDER_H_
#define CPP_SDK_FLAGD_PROVIDER_H_

#include <openfeature/provider.h>

#include <atomic>

#include "flagd/configuration.h"

namespace flagd {

class FlagdProvider : public openfeature::FeatureProvider {
 public:
  explicit FlagdProvider(FlagdProviderConfig config = FlagdProviderConfig());

  ~FlagdProvider() override;

  openfeature::Metadata GetMetadata() const override;

  void initialize();
  void shutdown();

  std::unique_ptr<openfeature::ProviderEvaluation<bool>> GetBooleanEvaluation(
      std::string_view flag, bool default_value,
      const openfeature::EvaluationContext& ctx) override;

  // TODO: Add other flag types (e.g. string, int, float, object)

 private:
  FlagdProviderConfig configuration;
  std::atomic<bool> isReady;
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_PROVIDER_H_
