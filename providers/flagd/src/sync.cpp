#include "flagd/sync.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>

#include "absl/status/status.h"
#include "flagd/configuration.h"
#include "flagd/sync/v1/sync.grpc.pb.h"

using namespace flagd::sync::v1;
using json = nlohmann::json;

namespace flagd {

FlagSync::FlagSync() {
  current_flags_ = std::make_shared<nlohmann::json>(nlohmann::json::object());
}

void FlagSync::UpdateFlags(const nlohmann::json& new_json) {
  auto new_snapshot = std::make_shared<const nlohmann::json>(new_json);
  {
    std::lock_guard<std::mutex> lock(flags_mutex_);
    current_flags_ = std::move(new_snapshot);
  }
}

std::shared_ptr<const nlohmann::json> FlagSync::GetFlags() const {
  std::lock_guard<std::mutex> lock(flags_mutex_);
  return current_flags_;
}

GrpcSync::GrpcSync(const flagd::FlagdProviderConfig& config) : config_(config) {
  std::string target = config_.get_effective_target_uri();

  // TODO(#11): Use tls from config_ and create secure channel
  channel_ = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
  stub_ = FlagSyncService::NewStub(channel_);
}

GrpcSync::~GrpcSync() { GrpcSync::Shutdown().IgnoreError(); }

absl::Status GrpcSync::Init(const openfeature::EvaluationContext& ctx) {
  shutdown_requested_ = false;
  background_thread_ = std::thread(&GrpcSync::WaitForUpdates, this);
  return absl::OkStatus();
}

absl::Status GrpcSync::Shutdown() {
  if (shutdown_requested_.exchange(true)) return absl::OkStatus();

  {
    std::lock_guard<std::mutex> lock(connection_mutex_);
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

  if (config_.get_provider_id().has_value() &&
      !config_.get_provider_id()->empty()) {
    request.set_provider_id(config_.get_provider_id()->data());
  }

  {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    if (shutdown_requested_) return;
    context_ = std::make_unique<grpc::ClientContext>();
  }

  auto reader = stub_->SyncFlags(context_.get(), request);
  SyncFlagsResponse response;

  while (reader->Read(&response)) {
    if (shutdown_requested_) break;

    try {
      auto raw = json::parse(response.flag_configuration());

      UpdateFlags(raw);
    } catch (const std::exception& e) {
      continue;  // TODO(#10): We should log an error here
    }
  }

  {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    context_.reset();
  }
}

}  // namespace flagd