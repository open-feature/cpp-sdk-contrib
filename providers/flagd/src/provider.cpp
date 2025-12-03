#include "flagd/provider.h"

#include <openfeature/provider.h>

#include "absl/status/status.h"
#include "flagd/configuration.h"

namespace flagd {

openfeature::Metadata FlagdProvider::GetMetadata() const {
  return openfeature::Metadata{"flagd"};
}

FlagdProvider::FlagdProvider(FlagdProviderConfig config)
    : configuration_(std::move(config)), is_ready_(false) {}

FlagdProvider::~FlagdProvider() {
  if (is_ready_) {
    Shutdown()
        .IgnoreError();  // We should probably Log this, once Logging is set up
  }
}

absl::Status FlagdProvider::Init(const openfeature::EvaluationContext& ctx) {
  return absl::UnimplementedError("Init is not implemented yet!");
}

absl::Status FlagdProvider::Shutdown() {
  if (!is_ready_) return absl::OkStatus();

  return absl::UnimplementedError("Shutdown is not implemented yet!");
}

std::unique_ptr<openfeature::ProviderEvaluation<bool>>
FlagdProvider::GetBooleanEvaluation(const std::string_view flag,
                                    bool default_value,
                                    const openfeature::EvaluationContext& ctx) {
  return nullptr;
}

}  // namespace flagd
