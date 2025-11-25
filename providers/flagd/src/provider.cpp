#include "flagd/provider.h"

#include <openfeature/provider.h>

#include "flagd/configuration.h"

namespace flagd {

openfeature::Metadata FlagdProvider::GetMetadata() const {
  return openfeature::Metadata{"flagd"};
}

FlagdProvider::FlagdProvider(FlagdProviderConfig config)
    : configuration(std::move(config)), isReady(false) {}

FlagdProvider::~FlagdProvider() {
  if (isReady) {
    shutdown();
  }
}

void FlagdProvider::initialize() { throw "Initialize is not implemented yet!"; }

void FlagdProvider::shutdown() {
  if (!isReady) return;

  throw "Shutdown is not implemented yet!";
}

std::unique_ptr<openfeature::ProviderEvaluation<bool>>
FlagdProvider::GetBooleanEvaluation(const std::string_view flag,
                                    bool defaultValue,
                                    const openfeature::EvaluationContext& ctx) {
  return nullptr;
}

}  // namespace flagd
