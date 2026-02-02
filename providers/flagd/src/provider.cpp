#include "provider.h"

#include <openfeature/provider.h>

#include <cstdlib>
#include <optional>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "flagd/configuration.h"
#include "flagd/sync.h"

namespace flagd {

openfeature::Metadata FlagdProvider::GetMetadata() const {
  return openfeature::Metadata{"flagd"};
}

FlagdProvider::FlagdProvider(FlagdProviderConfig config)
    : configuration_(std::move(config)), is_ready_(false) {
  if (configuration_.GetOfflineFlagSourcePath().has_value()) {
    // TODO(#20) Implement File sync
    LOG(FATAL) << "File sync is not implemented yet!";
  }

  sync_ = std::make_unique<GrpcSync>(configuration_);
}

FlagdProvider::~FlagdProvider() {
  if (is_ready_) {
    absl::Status status = FlagdProvider::Shutdown();
    if (!status.ok()) {
      LOG(ERROR) << "Error during FlagdProvider shutdown: " << status;
    }
  }
}

absl::Status FlagdProvider::Init(const openfeature::EvaluationContext& ctx) {
  if (is_ready_) return absl::OkStatus();

  absl::Status status = sync_->Init(ctx);
  if (status.ok()) {
    is_ready_ = true;
  }
  return status;
}

absl::Status FlagdProvider::Shutdown() {
  if (!is_ready_) return absl::OkStatus();

  absl::Status status = sync_->Shutdown();
  is_ready_ = false;
  return status;
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
