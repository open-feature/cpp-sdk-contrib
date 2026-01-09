#include "sync.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <thread>

#include "absl/status/status.h"
#include "flagd/configuration.h"
#include "flagd/sync/v1/sync.grpc.pb.h"

using ::flagd::sync::v1::FlagSyncService;
using ::flagd::sync::v1::SyncFlagsRequest;
using ::flagd::sync::v1::SyncFlagsResponse;
using Json = nlohmann::json;

namespace flagd {

FlagSync::FlagSync() {
  current_flags_ = std::make_shared<nlohmann::json>(nlohmann::json::object());
}

void FlagSync::UpdateFlags(const nlohmann::json& new_json) {
  auto new_snapshot = std::make_shared<const nlohmann::json>(new_json);
  {
    std::scoped_lock lock(flags_mutex_);
    current_flags_ = std::move(new_snapshot);
  }
}

std::shared_ptr<const nlohmann::json> FlagSync::GetFlags() const {
  std::scoped_lock lock(flags_mutex_);
  return current_flags_;
}

GrpcSync::GrpcSync(FlagdProviderConfig config) : config_(std::move(config)) {}

GrpcSync::~GrpcSync() { GrpcSync::Shutdown().IgnoreError(); }

absl::Status GrpcSync::Init(const openfeature::EvaluationContext& ctx) {
  std::string target = config_.GetEffectiveTargetUri();
  absl::StatusOr<std::shared_ptr<grpc::ChannelCredentials>> creds =
      config_.GetEffectiveCredentials();

  if (!creds.ok()) return creds.status();

  channel_ = grpc::CreateChannel(target, *creds);
  stub_ = FlagSyncService::NewStub(channel_);

  shutdown_requested_ = false;
  background_thread_ = std::thread(&GrpcSync::WaitForUpdates, this);
  return absl::OkStatus();
}

absl::Status GrpcSync::Shutdown() {
  if (shutdown_requested_.exchange(true)) return absl::OkStatus();

  {
    std::scoped_lock lock(connection_mutex_);
    if (context_) context_->TryCancel();
  }

  if (background_thread_.joinable()) {
    background_thread_.join();
  }

  return absl::OkStatus();
}

void GrpcSync::WaitForUpdates() {
  // TODO(#12) Add automatic reconection with exponential backoff
  SyncFlagsRequest request;

  if (config_.GetProviderId().has_value() &&
      !config_.GetProviderId()->empty()) {
    request.set_provider_id(config_.GetProviderId()->data());
  }

  {
    std::scoped_lock lock(connection_mutex_);
    if (shutdown_requested_) return;
    context_ = std::make_unique<grpc::ClientContext>();
  }

  auto reader = stub_->SyncFlags(context_.get(), request);
  SyncFlagsResponse response;

  while (reader->Read(&response)) {
    if (shutdown_requested_) break;

    try {
      Json raw = Json::parse(response.flag_configuration());

      UpdateFlags(raw);
    } catch (const std::exception& e) {
      continue;  // TODO(#10): We should log an error here
    }
  }

  {
    std::scoped_lock lock(connection_mutex_);
    context_.reset();
  }
}

}  // namespace flagd