#include "provider.h"

#include <openfeature/provider.h>

#include <optional>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "flagd/configuration.h"
#include "flagd/sync/grpc/grpc_sync.h"

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

  sync_ = std::make_shared<GrpcSync>(configuration_);

  evaluator_ = std::make_unique<JsonLogicEvaluator>(sync_);
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
  if (!is_ready_) {
    return std::make_unique<openfeature::BoolResolutionDetails>(
        default_value, openfeature::Reason::kError, "",
        openfeature::FlagMetadata(), openfeature::ErrorCode::kProviderNotReady,
        "Provider not ready");
  }
  return evaluator_->ResolveBoolean(flag, default_value, ctx);
}

std::unique_ptr<openfeature::StringResolutionDetails>
FlagdProvider::GetStringEvaluation(const std::string_view flag,
                                   std::string_view default_value,
                                   const openfeature::EvaluationContext& ctx) {
  if (!is_ready_) {
    return std::make_unique<openfeature::StringResolutionDetails>(
        std::string(default_value), openfeature::Reason::kError, "",
        openfeature::FlagMetadata(), openfeature::ErrorCode::kProviderNotReady,
        "Provider not ready");
  }
  return evaluator_->ResolveString(flag, default_value, ctx);
}

std::unique_ptr<openfeature::IntResolutionDetails>
FlagdProvider::GetIntegerEvaluation(const std::string_view flag,
                                    int64_t default_value,
                                    const openfeature::EvaluationContext& ctx) {
  if (!is_ready_) {
    return std::make_unique<openfeature::IntResolutionDetails>(
        default_value, openfeature::Reason::kError, "",
        openfeature::FlagMetadata(), openfeature::ErrorCode::kProviderNotReady,
        "Provider not ready");
  }
  return evaluator_->ResolveInteger(flag, default_value, ctx);
}

std::unique_ptr<openfeature::DoubleResolutionDetails>
FlagdProvider::GetDoubleEvaluation(const std::string_view flag,
                                   double default_value,
                                   const openfeature::EvaluationContext& ctx) {
  if (!is_ready_) {
    return std::make_unique<openfeature::DoubleResolutionDetails>(
        default_value, openfeature::Reason::kError, "",
        openfeature::FlagMetadata(), openfeature::ErrorCode::kProviderNotReady,
        "Provider not ready");
  }
  return evaluator_->ResolveDouble(flag, default_value, ctx);
}

std::unique_ptr<openfeature::ObjectResolutionDetails>
FlagdProvider::GetObjectEvaluation(const std::string_view flag,
                                   openfeature::Value default_value,
                                   const openfeature::EvaluationContext& ctx) {
  if (!is_ready_) {
    return std::make_unique<openfeature::ObjectResolutionDetails>(
        std::move(default_value), openfeature::Reason::kError, "",
        openfeature::FlagMetadata(), openfeature::ErrorCode::kProviderNotReady,
        "Provider not ready");
  }
  return evaluator_->ResolveObject(flag, std::move(default_value), ctx);
}

}  // namespace flagd
