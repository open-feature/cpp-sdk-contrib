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

  // Main loop for the background thread. Manages stream lifecycle,
  // reconnection, backoff, and grace periods.
  void WaitForUpdates();

  // Internal implementation of shutdown. Safely cancels the gRPC context
  // and joins the background thread.
  // Acquires lifecycle_mutex_ internally, but will release it
  // temporarily to join the thread.
  absl::Status ShutdownInternal(State target_state);

  // Initializes the gRPC client reader stream.
  // Acquires lifecycle_mutex_ internally to safely update the active gRPC
  // context.
  absl::StatusOr<
      std::unique_ptr<grpc::ClientReader<::flagd::sync::v1::SyncFlagsResponse>>>
  InitStream(const ::flagd::sync::v1::SyncFlagsRequest& request);

  // Helper to create the gRPC stub.
  absl::StatusOr<std::unique_ptr<::flagd::sync::v1::FlagSyncService::Stub>>
  CreateStub();

  // Spawns the background thread running WaitForUpdates.
  absl::Status StartBackgroundThread();

  // Blocks the calling thread until the sync state becomes kReady,
  // or the initialization timeout is reached.
  // Expects lifecycle_mutex_ to be held via the passed lock, and will
  // release/re-acquire it while waiting on the condition variable.
  absl::Status WaitForInitialization(std::unique_lock<std::mutex>& lock);

  struct StreamResult {
    grpc::Status status;
    bool data_received = false;
  };

  // Processes incoming messages from the gRPC stream.
  // Returns a StreamResult containing the status and whether any data was
  // received.
  StreamResult ProcessStream(
      grpc::ClientReader<::flagd::sync::v1::SyncFlagsResponse>* reader);

  // Helper to check if we should stop the background thread loop.
  bool ShouldStop() const;

  // Executes a single stream session.
  // Returns a StreamResult containing the status and whether any data was
  // received.
  StreamResult ExecuteStream();

  // Helper to check if a gRPC status code is considered fatal.
  bool IsFatalError(const grpc::Status& status) const;

  // Handles fatal errors by clearing flags and transitioning to kFatal state.
  void HandleFatalError(const grpc::Status& status);

  // Handles non-fatal errors by transitioning to kReconnecting state.
  void HandleNonFatalError(const grpc::Status& status);

  // Checks if the disconnected duration has exceeded the grace period,
  // and clears flags if it has.
  void CheckGracePeriod(
      std::chrono::steady_clock::time_point last_healthy_time);

  // Waits for the retry backoff period or until shutdown/fatal state is
  // reached. Returns true if shutdown or fatal state was reached.
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
