#include "provider.h"

#include <openfeature/provider.h>

#include <optional>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "configuration.h"

namespace flagd {

openfeature::Metadata FlagdProvider::GetMetadata() const {
  return openfeature::Metadata{"flagd"};
}

FlagdProvider::FlagdProvider(FlagdProviderConfig config)
    : configuration_(std::move(config)), is_ready_(false) {}

FlagdProvider::~FlagdProvider() {
  if (is_ready_) {
    absl::Status status = Shutdown();
    if (!status.ok()) {
      LOG(ERROR) << "Error during FlagdProvider shutdown: " << status;
    }
  }
}

absl::Status FlagdProvider::Init(const openfeature::EvaluationContext& ctx) {
  return absl::UnimplementedError("Init is not implemented yet!");
}

absl::Status FlagdProvider::Shutdown() {
  if (!is_ready_) return absl::OkStatus();

  return absl::UnimplementedError("Shutdown is not implemented yet!");
}

std::unique_ptr<openfeature::BoolResolutionDetails>
FlagdProvider::GetBooleanEvaluation(const std::string_view flag,
                                    bool default_value,
                                    const openfeature::EvaluationContext& ctx) {
  return std::make_unique<openfeature::BoolResolutionDetails>(
      default_value, openfeature::Reason::kDefault, "default-variant",
      openfeature::FlagMetadata(), std::nullopt, std::nullopt);
}

}  // namespace flagd
