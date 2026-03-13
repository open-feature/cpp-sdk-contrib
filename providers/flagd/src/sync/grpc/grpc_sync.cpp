#include "grpc_sync.h"

#include <grpcpp/grpcpp.h>

#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "flagd/configuration.h"
#include "flagd/sync/v1/sync.grpc.pb.h"

using ::flagd::sync::v1::FlagSyncService;
using ::flagd::sync::v1::SyncFlagsRequest;
using ::flagd::sync::v1::SyncFlagsResponse;
using Json = nlohmann::json;

namespace flagd {

GrpcSync::GrpcSync(FlagdProviderConfig config) : config_(std::move(config)) {}

GrpcSync::~GrpcSync() {
  absl::Status status = GrpcSync::Shutdown();
  if (!status.ok()) {
    LOG(ERROR) << "Error during GrpcSync shutdown: " << status;
  }
}

absl::Status GrpcSync::Init(const openfeature::EvaluationContext& ctx) {
  std::unique_lock lock(lifecycle_mutex_);

  switch (state_) {
    case State::kReady:
      return absl::OkStatus();
    case State::kInitializing:
      lifecycle_cv_.wait(lock,
                         [this] { return state_ != State::kInitializing; });
      return (state_ == State::kReady) ? absl::OkStatus() : init_result_;
    case State::kShuttingDown:
      lifecycle_cv_.wait(lock,
                         [this] { return state_ == State::kUninitialized; });
      break;
    case State::kUninitialized:
      break;
  }

  state_ = State::kInitializing;
  init_result_ = absl::UnknownError("Initialization incomplete");

  std::string target = config_.GetEffectiveTargetUri();
  absl::StatusOr<std::shared_ptr<grpc::ChannelCredentials>> creds =
      config_.GetEffectiveCredentials();
  if (!creds.ok()) {
    state_ = State::kUninitialized;
    return creds.status();
  }

  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(target, *creds);
  stub_ = FlagSyncService::NewStub(channel);

  context_ = std::make_shared<grpc::ClientContext>();

  try {
    background_thread_ = std::thread(&GrpcSync::WaitForUpdates, this);
  } catch (const std::exception& e) {
    state_ = State::kUninitialized;
    return absl::InternalError(
        absl::StrCat("Failed to spawn thread: ", e.what()));
  }

  lifecycle_cv_.wait(lock, [this] { return state_ != State::kInitializing; });

  if (state_ != State::kReady) {
    if (background_thread_.joinable()) {
      lock.unlock();
      background_thread_.join();
      lock.lock();
    }
    return init_result_;
  }

  return absl::OkStatus();
}

absl::Status GrpcSync::Shutdown() {
  std::unique_lock lock(lifecycle_mutex_);

  if (state_ == State::kUninitialized) {
    return absl::OkStatus();
  }

  if (state_ == State::kShuttingDown) {
    lifecycle_cv_.wait(lock,
                       [this] { return state_ == State::kUninitialized; });
    return absl::OkStatus();
  }

  State previous_state = state_;
  state_ = State::kShuttingDown;

  if (context_) {
    context_->TryCancel();
  }

  lock.unlock();
  if (background_thread_.joinable()) {
    background_thread_.join();
  }
  lock.lock();

  context_.reset();
  stub_.reset();
  state_ = State::kUninitialized;

  if (previous_state == State::kInitializing) {
    init_result_ =
        absl::CancelledError("Shutdown called during initialization");
  }

  lifecycle_cv_.notify_all();

  return absl::OkStatus();
}

void GrpcSync::WaitForUpdates() {
  // TODO(#12) Add automatic reconection with exponential backoff
  SyncFlagsRequest request;

  if (config_.GetProviderId().has_value() &&
      !config_.GetProviderId()->empty()) {
    request.set_provider_id(*config_.GetProviderId());
  }

  std::shared_ptr<grpc::ClientContext> local_ctx;
  {
    std::scoped_lock lock(lifecycle_mutex_);
    local_ctx = context_;
  }

  if (!local_ctx) return;

  auto reader = stub_->SyncFlags(local_ctx.get(), request);
  SyncFlagsResponse response;

  bool first_read = true;
  while (reader->Read(&response)) {
    try {
      Json raw = Json::parse(response.flag_configuration());

      UpdateFlags(raw);

      if (first_read) {
        std::scoped_lock lock(lifecycle_mutex_);
        if (state_ == State::kInitializing) {
          state_ = State::kReady;
          init_result_ = absl::OkStatus();
          lifecycle_cv_.notify_all();
        }
        first_read = false;
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << "Failed to parse flag configuration: " << e.what();
      if (first_read) {
        std::scoped_lock lock(lifecycle_mutex_);
        if (state_ == State::kInitializing) {
          state_ = State::kUninitialized;
          init_result_ =
              absl::InternalError(absl::StrCat("Parse error: ", e.what()));
          lifecycle_cv_.notify_all();
        }
        // If we fail first read, we abort the stream
        return;
      }
    }
  }

  grpc::Status status = reader->Finish();

  {
    std::scoped_lock lock(lifecycle_mutex_);
    if (state_ == State::kInitializing) {
      state_ = State::kUninitialized;
      init_result_ = status.ok()
                         ? absl::InternalError("Stream closed immediately")
                         : absl::InternalError(status.error_message());
      lifecycle_cv_.notify_all();
    }
  }
}

}  // namespace flagd
