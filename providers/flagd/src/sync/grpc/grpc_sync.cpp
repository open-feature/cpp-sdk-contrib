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
      if (!lifecycle_cv_.wait_for(
              lock, std::chrono::milliseconds(config_.GetDeadlineMs()),
              [this] { return state_ != State::kInitializing; })) {
        init_timed_out_ = true;
        init_result_ = absl::DeadlineExceededError("Initialization timed out");
        lock.unlock();
        static_cast<void>(ShutdownInternal(State::kUninitialized));
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

  std::string target = config_.GetEffectiveTargetUri();
  absl::StatusOr<std::shared_ptr<grpc::ChannelCredentials>> creds =
      config_.GetEffectiveCredentials();
  if (!creds.ok()) {
    state_ = State::kUninitialized;
    LOG(INFO) << "GrpcSync state changed to kUninitialized";
    return creds.status();
  }

  std::shared_ptr<grpc::Channel> channel;
  grpc::ChannelArguments args;
  args.SetServiceConfigJSON(config_.GetServiceConfigJson());
  if (config_.GetKeepAliveTimeMs() > 0) {
    args.SetInt("grpc.keepalive_time_ms", config_.GetKeepAliveTimeMs());
  }
  channel = grpc::CreateCustomChannel(target, *creds, args);
  stub_ = FlagSyncService::NewStub(channel);

  try {
    background_thread_ = std::thread(&GrpcSync::WaitForUpdates, this);
  } catch (const std::exception& e) {
    state_ = State::kUninitialized;
    LOG(INFO) << "GrpcSync state changed to kUninitialized";
    return absl::InternalError(
        absl::StrCat("Failed to spawn thread: ", e.what()));
  }

  if (!lifecycle_cv_.wait_for(
          lock, std::chrono::milliseconds(config_.GetDeadlineMs()),
          [this] { return state_ != State::kInitializing; })) {
    init_timed_out_ = true;
    init_result_ = absl::DeadlineExceededError("Initialization timed out");
    lock.unlock();
    static_cast<void>(ShutdownInternal(State::kUninitialized));
    return init_result_;
  }

  if (state_ != State::kReady) {
    State final_state = state_;
    lock.unlock();
    static_cast<void>(ShutdownInternal(final_state));
    return init_result_;
  }

  return absl::OkStatus();
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

    if (context_) {
      context_->TryCancel();
    }
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

  if (previous_state == State::kInitializing &&
      target_state == State::kUninitialized) {
    init_result_ =
        absl::CancelledError("Shutdown called during initialization");
  }

  lifecycle_cv_.notify_all();

  return absl::OkStatus();
}

void GrpcSync::WaitForUpdates() {
  auto last_healthy_time = std::chrono::steady_clock::now();
  int retry_count = 0;

  while (true) {
    {
      std::scoped_lock lock(lifecycle_mutex_);
      if (state_ == State::kShuttingDown || state_ == State::kFatal) break;
    }

    SyncFlagsRequest request;
    if (config_.GetProviderId().has_value() &&
        !config_.GetProviderId()->empty()) {
      request.set_provider_id(*config_.GetProviderId());
    }

    std::shared_ptr<grpc::ClientContext> local_ctx =
        std::make_shared<grpc::ClientContext>();
    {
      std::scoped_lock lock(lifecycle_mutex_);
      if (state_ == State::kShuttingDown) break;
      context_ = local_ctx;
    }
    if (config_.GetStreamDeadlineMs() > 0) {
      auto deadline = std::chrono::system_clock::now() +
                      std::chrono::milliseconds(config_.GetStreamDeadlineMs());
      local_ctx->set_deadline(deadline);
    }

    auto reader = stub_->SyncFlags(local_ctx.get(), request);

    if (!reader) {
      LOG(ERROR) << "Failed to create sync stream";
      std::unique_lock lock(lifecycle_mutex_);
      if (state_ == State::kInitializing) {
        state_ = State::kReconnecting;
        LOG(INFO) << "GrpcSync state changed to kReconnecting";
        init_result_ = absl::InternalError("Failed to create stream");
        lifecycle_cv_.notify_all();
      }
      lifecycle_cv_.wait_for(
          lock, std::chrono::milliseconds(config_.GetRetryBackoffMs()), [this] {
            return state_ == State::kShuttingDown || state_ == State::kFatal;
          });
      continue;
    }

    SyncFlagsResponse response;
    bool connected = false;

    while (reader->Read(&response)) {
      try {
        Json raw = Json::parse(response.flag_configuration());
        UpdateFlags(raw);

        if (!connected) {
          connected = true;
          retry_count = 0;
          std::scoped_lock lock(lifecycle_mutex_);
          if (state_ == State::kInitializing ||
              state_ == State::kReconnecting) {
            state_ = State::kReady;
            LOG(INFO) << "GrpcSync state changed to kReady";
            init_result_ = absl::OkStatus();
            lifecycle_cv_.notify_all();
            // TODO: emit PROVIDER_READY
            // TODO: emit PROVIDER_CONFIGURATION_CHANGED
          }
        }
        // TODO: emit PROVIDER_CONFIGURATION_CHANGED
      } catch (const std::exception& e) {
        LOG(ERROR) << "Failed to parse flag configuration: " << e.what();
      }
    }

    grpc::Status status = reader->Finish();
    LOG(WARNING) << "Sync stream closed: " << status.error_message()
                 << " (code: " << status.error_code() << ")";

    if (connected) {
      last_healthy_time = std::chrono::steady_clock::now();
    }

    // Check fatal status codes
    const auto& fatal_codes = config_.GetFatalStatusCodes();
    bool is_fatal = std::find(fatal_codes.cbegin(), fatal_codes.cend(),
                              status.error_code()) != fatal_codes.cend();

    if (is_fatal) {
      ClearFlags();
      std::scoped_lock lock(lifecycle_mutex_);
      state_ = State::kFatal;
      LOG(INFO) << "GrpcSync state changed to kFatal";
      init_result_ = absl::InternalError(
          absl::StrCat("Fatal gRPC error: ", status.error_message()));
      lifecycle_cv_.notify_all();
      // TODO: emit PROVIDER_FATAL
      break;
    }

    {
      std::scoped_lock lock(lifecycle_mutex_);
      if (state_ == State::kInitializing) {
        state_ = State::kReconnecting;
        LOG(INFO) << "GrpcSync state changed to kReconnecting";
        init_result_ = absl::InternalError(
            absl::StrCat("Stream failed: ", status.error_message()));
        lifecycle_cv_.notify_all();
      } else if (state_ == State::kReady) {
        state_ = State::kReconnecting;
        LOG(INFO) << "GrpcSync state changed to kReconnecting";
        // TODO: emit PROVIDER_STALE
      }
    }

    int64_t backoff = config_.GetRetryBackoffMs() * (1LL << retry_count);
    if (backoff >= config_.GetRetryBackoffMaxMs()) {
      backoff = config_.GetRetryBackoffMaxMs();
    } else {
      retry_count++;
    }

    auto now = std::chrono::steady_clock::now();
    auto disconnected_duration =
        std::chrono::duration_cast<std::chrono::seconds>(now -
                                                         last_healthy_time);
    if (disconnected_duration.count() > config_.GetRetryGracePeriod()) {
      ClearFlags();
      // TODO: emit PROVIDER_ERROR
    }

    {
      std::unique_lock lock(lifecycle_mutex_);
      lifecycle_cv_.wait_for(lock, std::chrono::milliseconds(backoff), [this] {
        return state_ == State::kShuttingDown || state_ == State::kFatal;
      });
    }
  }
}

}  // namespace flagd
