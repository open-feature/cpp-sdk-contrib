#ifndef CPP_SDK_FLAGD_SYNC_GRPC_SYNC_H_
#define CPP_SDK_FLAGD_SYNC_GRPC_SYNC_H_

#include <grpcpp/grpcpp.h>

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
  void WaitForUpdates();

  std::unique_ptr<flagd::sync::v1::FlagSyncService::Stub> stub_;
  std::shared_ptr<grpc::ClientContext> context_;
  enum class State : uint8_t {
    kUninitialized,
    kInitializing,  // Thread started, waiting for first connection
    kReady,         // First sync complete, running normally
    kShuttingDown,  // Shutdown requested, cleaning up
  };

  std::thread background_thread_;

  std::mutex lifecycle_mutex_;
  std::condition_variable lifecycle_cv_;
  State state_ = State::kUninitialized;
  absl::Status init_result_;

  FlagdProviderConfig config_;
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_SYNC_GRPC_SYNC_H_
