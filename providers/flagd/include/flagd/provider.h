#ifndef CPP_SDK_FLAGD_PROVIDER_H_
#define CPP_SDK_FLAGD_PROVIDER_H_

#include "openfeature/provider.h"

namespace flagd {

class FlagdProvider : public openfeature::FeatureProvider {
 public:
  ~FlagdProvider() = default;
  openfeature::Metadata GetMetadata() const;
  std::unique_ptr<openfeature::ProviderEvaluation<bool>> GetBooleanEvaluation(
      const std::string& flag, bool defaultValue,
      const openfeature::EvaluationContext& ctx);

  // TODO: Add other flag types (e.g. string, int, float, object)
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_PROVIDER_H_