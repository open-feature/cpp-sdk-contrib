#ifndef CPP_SDK_FLAGD_SYNC_GRPC_SYNC_H_
#define CPP_SDK_FLAGD_SYNC_GRPC_SYNC_H_

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "absl/status/status.h"
#include "flagd/configuration.h"
#include "flagd/sync/sync.h"
#include "flagd/sync/v1/sync.grpc.pb.h"
#include "openfeature/evaluation_context.h"

namespace flagd {

class GrpcSync final : public FlagSync {
 public:
  explicit GrpcSync(FlagdProviderConfig config);
  ~GrpcSync() override;

  absl::Status Init(const openfeature::EvaluationContext& ctx) override;
  absl::Status Shutdown() override;

 private:
  enum class State : uint8_t {
    kUninitialized,
    kInitializing,  // Thread started, waiting for first connection
    kReady,         // First sync complete, running normally
    kReconnecting,  // Connection lost or failed, trying to reconnect
    kShuttingDown,  // Shutdown requested, cleaning up
    kFatal,         // Fatal error, gives up
  };

  void WaitForUpdates();
  absl::Status ShutdownInternal(State target_state);
  absl::StatusOr<
      std::unique_ptr<grpc::ClientReader<::flagd::sync::v1::SyncFlagsResponse>>>
  InitStream(const ::flagd::sync::v1::SyncFlagsRequest& request);
  absl::StatusOr<std::unique_ptr<::flagd::sync::v1::FlagSyncService::Stub>>
  CreateStub();
  absl::Status StartBackgroundThread();
  absl::Status WaitForInitialization(std::unique_lock<std::mutex>& lock);
  grpc::Status ProcessStream(
      grpc::ClientReader<::flagd::sync::v1::SyncFlagsResponse>* reader,
      bool& stream_success);
  bool ShouldStop() const;
  grpc::Status ExecuteStream(bool& stream_success);
  bool IsFatalError(const grpc::Status& status) const;
  void HandleFatalError(const grpc::Status& status);
  void HandleNonFatalError(const grpc::Status& status);
  void CheckGracePeriod(
      std::chrono::steady_clock::time_point last_healthy_time);
  bool WaitForBackoff();

  std::unique_ptr<flagd::sync::v1::FlagSyncService::Stub> stub_;
  std::shared_ptr<grpc::ClientContext> context_;

  std::thread background_thread_;

  mutable std::mutex lifecycle_mutex_;
  std::condition_variable lifecycle_cv_;
  State state_ = State::kUninitialized;
  absl::Status init_result_;
  bool init_timed_out_ = false;

  FlagdProviderConfig config_;
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_SYNC_GRPC_SYNC_H_
