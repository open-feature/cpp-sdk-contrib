#ifndef CPP_SDK_FLAGD_PROVIDER_H_
#define CPP_SDK_FLAGD_PROVIDER_H_

#include "openfeature/provider.h"

namespace flagd {

class FlagdProvider : public openfeature::FeatureProvider {
 public:
  ~FlagdProvider() override = default;
  openfeature::Metadata GetMetadata() const override;
  std::unique_ptr<openfeature::ProviderEvaluation<bool>> GetBooleanEvaluation(
      std::string_view flag, bool default_value,
      const openfeature::EvaluationContext& ctx) override;

  // TODO: Add other flag types (e.g. string, int, float, object)
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_PROVIDER_H_
