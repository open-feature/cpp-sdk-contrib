#include "grpc_sync.h"

#include <grpcpp/grpcpp.h>

#include <algorithm>
#include <chrono>
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
      if (init_timed_out_) {
        return absl::DeadlineExceededError("Initialization timed out");
      }
      if (!lifecycle_cv_.wait_for(
              lock, std::chrono::milliseconds(config_.GetDeadlineMs()),
              [this] { return state_ != State::kInitializing; })) {
        init_timed_out_ = true;
        init_result_ = absl::DeadlineExceededError("Initialization timed out");
        lock.unlock();
        return init_result_;
      }
      return (state_ == State::kReady) ? absl::OkStatus() : init_result_;
    case State::kReconnecting:
      if (init_timed_out_) {
        return absl::DeadlineExceededError("Initialization timed out");
      }
      return init_result_;
    case State::kShuttingDown:
      lifecycle_cv_.wait(lock,
                         [this] { return state_ == State::kUninitialized; });
      break;
    case State::kFatal:
      return init_result_;
    case State::kUninitialized:
      break;
  }

  state_ = State::kInitializing;
  LOG(INFO) << "GrpcSync state changed to kInitializing";
  init_result_ = absl::UnknownError("Initialization incomplete");

  auto stub_res = CreateStub();
  if (!stub_res.ok()) {
    state_ = State::kUninitialized;
    LOG(INFO) << "GrpcSync state changed to kUninitialized";
    return stub_res.status();
  }
  stub_ = std::move(stub_res.value());

  auto thread_status = StartBackgroundThread();
  if (!thread_status.ok()) {
    state_ = State::kUninitialized;
    LOG(INFO) << "GrpcSync state changed to kUninitialized";
    return thread_status;
  }

  return WaitForInitialization(lock);
}

absl::Status GrpcSync::Shutdown() {
  return ShutdownInternal(State::kUninitialized);
}

absl::Status GrpcSync::ShutdownInternal(State target_state) {
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
  if (state_ != State::kFatal) {
    state_ = State::kShuttingDown;
    LOG(INFO) << "GrpcSync state changed to kShuttingDown";

    if (previous_state == State::kInitializing &&
        target_state == State::kUninitialized) {
      init_result_ =
          absl::CancelledError("Shutdown called during initialization");
    }

    if (context_) {
      context_->TryCancel();
    }
    lifecycle_cv_.notify_all();
  }

  lock.unlock();
  if (background_thread_.joinable()) {
    background_thread_.join();
  }
  lock.lock();

  context_.reset();
  stub_.reset();
  state_ = target_state;
  LOG(INFO) << "GrpcSync state changed (ShutdownInternal)";

  lifecycle_cv_.notify_all();

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<::flagd::sync::v1::FlagSyncService::Stub>>
GrpcSync::CreateStub() {
  std::string target = config_.GetEffectiveTargetUri();
  absl::StatusOr<std::shared_ptr<grpc::ChannelCredentials>> creds =
      config_.GetEffectiveCredentials();
  if (!creds.ok()) {
    return creds.status();
  }

  std::shared_ptr<grpc::Channel> channel;
  grpc::ChannelArguments args;
  args.SetServiceConfigJSON(config_.GetServiceConfigJson());

  if (config_.GetKeepAliveTimeMs() > 0) {
    args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, config_.GetKeepAliveTimeMs());
  }

  if (config_.GetRetryBackoffMs() > 0) {
    args.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS,
                config_.GetRetryBackoffMs());
  }
  if (config_.GetRetryBackoffMaxMs() > 0) {
    args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS,
                config_.GetRetryBackoffMaxMs());
  }

  channel = grpc::CreateCustomChannel(target, *creds, args);
  return FlagSyncService::NewStub(channel);
}

absl::Status GrpcSync::StartBackgroundThread() {
  try {
    background_thread_ = std::thread(&GrpcSync::WaitForUpdates, this);
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrCat("Failed to spawn thread: ", e.what()));
  }
  return absl::OkStatus();
}

absl::Status GrpcSync::WaitForInitialization(
    std::unique_lock<std::mutex>& lock) {
  if (!lifecycle_cv_.wait_for(
          lock, std::chrono::milliseconds(config_.GetDeadlineMs()),
          [this] { return state_ != State::kInitializing; })) {
    init_timed_out_ = true;
    init_result_ = absl::DeadlineExceededError("Initialization timed out");
    return init_result_;
  }

  if (state_ != State::kReady) {
    return init_result_;
  }

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<grpc::ClientReader<SyncFlagsResponse>>>
GrpcSync::InitStream(const SyncFlagsRequest& request) {
  std::shared_ptr<grpc::ClientContext> local_ctx =
      std::make_shared<grpc::ClientContext>();
  local_ctx->set_wait_for_ready(true);

  if (config_.GetSelector().has_value() && !config_.GetSelector()->empty()) {
    local_ctx->AddMetadata("flagd-selector", *config_.GetSelector());
  }

  {
    std::scoped_lock lock(lifecycle_mutex_);
    if (state_ == State::kShuttingDown) {
      return absl::CancelledError("Shutdown requested");
    }
    context_ = local_ctx;
  }

  auto reader = stub_->SyncFlags(local_ctx.get(), request);
  if (!reader) {
    return absl::InternalError("Failed to create sync stream");
  }
  return reader;
}

grpc::Status GrpcSync::ProcessStream(
    grpc::ClientReader<SyncFlagsResponse>* reader, bool& stream_success) {
  SyncFlagsResponse response;
  bool connected = false;

  while (reader->Read(&response)) {
    stream_success = true;

    Json raw = Json::parse(response.flag_configuration(), nullptr, false);
    if (raw.is_discarded()) {
      LOG(ERROR) << "Failed to parse flag configuration: Invalid JSON";
      continue;
    }

    UpdateFlags(raw);

    if (!connected) {
      connected = true;
      std::scoped_lock lock(lifecycle_mutex_);
      if (state_ == State::kInitializing || state_ == State::kReconnecting) {
        state_ = State::kReady;
        init_timed_out_ = false;
        LOG(INFO) << "GrpcSync state changed to kReady";
        init_result_ = absl::OkStatus();
        lifecycle_cv_.notify_all();
        // TODO(#89): emit PROVIDER_READY
      }
    }
    // TODO(#89): emit PROVIDER_CONFIGURATION_CHANGED
  }

  return reader->Finish();
}

bool GrpcSync::ShouldStop() const {
  std::scoped_lock lock(lifecycle_mutex_);
  return state_ == State::kShuttingDown || state_ == State::kFatal;
}

grpc::Status GrpcSync::ExecuteStream(bool& stream_success) {
  SyncFlagsRequest request;
  if (config_.GetProviderId().has_value() &&
      !config_.GetProviderId()->empty()) {
    request.set_provider_id(*config_.GetProviderId());
  }

  absl::StatusOr<std::unique_ptr<grpc::ClientReader<SyncFlagsResponse>>>
      stream_res = InitStream(request);

  grpc::Status status;
  if (!stream_res.ok()) {
    LOG(ERROR) << stream_res.status().message();
    status = grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(stream_res.status().message()));
  } else {
    status = ProcessStream(stream_res.value().get(), stream_success);
    LOG(WARNING) << "Sync stream closed: " << status.error_message()
                 << " (code: " << status.error_code() << ")";
  }
  return status;
}

bool GrpcSync::IsFatalError(const grpc::Status& status) const {
  const std::vector<int>& fatal_codes = config_.GetFatalStatusCodes();
  return std::find(fatal_codes.cbegin(), fatal_codes.cend(),
                   status.error_code()) != fatal_codes.cend();
}

void GrpcSync::HandleFatalError(const grpc::Status& status) {
  ClearFlags();
  std::scoped_lock lock(lifecycle_mutex_);
  state_ = State::kFatal;
  LOG(INFO) << "GrpcSync state changed to kFatal";
  init_result_ = absl::InternalError(
      absl::StrCat("Fatal gRPC error: ", status.error_message()));
  lifecycle_cv_.notify_all();
  // TODO(#89): emit PROVIDER_FATAL
}

void GrpcSync::HandleNonFatalError(const grpc::Status& status) {
  std::scoped_lock lock(lifecycle_mutex_);
  if (state_ == State::kInitializing || state_ == State::kReady) {
    bool was_initializing = (state_ == State::kInitializing);
    state_ = State::kReconnecting;
    LOG(INFO) << "GrpcSync state changed to kReconnecting";
    if (was_initializing) {
      init_result_ = absl::InternalError(
          absl::StrCat("Stream failed: ", status.error_message()));
      lifecycle_cv_.notify_all();
    }
  }
}

void GrpcSync::CheckGracePeriod(
    std::chrono::steady_clock::time_point last_healthy_time) {
  std::chrono::time_point now = std::chrono::steady_clock::now();
  std::chrono::duration<int64_t> disconnected_duration =
      std::chrono::duration_cast<std::chrono::seconds>(now - last_healthy_time);
  if (disconnected_duration.count() > config_.GetRetryGracePeriod()) {
    ClearFlags();
    // TODO(#89): emit PROVIDER_ERROR
  }
}

bool GrpcSync::WaitForBackoff() {
  int64_t backoff = config_.GetRetryBackoffMaxMs();
  std::unique_lock lock(lifecycle_mutex_);
  return lifecycle_cv_.wait_for(
      lock, std::chrono::milliseconds(backoff), [this] {
        return state_ == State::kShuttingDown || state_ == State::kFatal;
      });
}

void GrpcSync::WaitForUpdates() {
  std::chrono::time_point last_healthy_time = std::chrono::steady_clock::now();

  while (!ShouldStop()) {
    bool stream_success = false;
    grpc::Status status = ExecuteStream(stream_success);

    if (stream_success) {
      last_healthy_time = std::chrono::steady_clock::now();
    }

    if (IsFatalError(status)) {
      HandleFatalError(status);
      break;
    }

    HandleNonFatalError(status);
    CheckGracePeriod(last_healthy_time);

    if (WaitForBackoff()) break;
  }
}

}  // namespace flagd
