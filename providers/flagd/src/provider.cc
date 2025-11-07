#include "openfeature/provider.h"

#include "flagd/provider.h"

namespace flagd {

openfeature::Metadata FlagdProvider::GetMetadata() const {
  return openfeature::Metadata{"flagd"};
}

std::unique_ptr<openfeature::ProviderEvaluation<bool>>
FlagdProvider::GetBooleanEvaluation(const std::string& flag, bool defaultValue,
                                    const openfeature::EvaluationContext& ctx) {
  return nullptr;
}

}  // namespace flagd