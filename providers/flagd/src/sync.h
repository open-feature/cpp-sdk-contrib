#ifndef CPP_SDK_FLAGD_SYNC_H_
#define CPP_SDK_FLAGD_SYNC_H_

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>

#include "absl/status/status.h"
#include "flagd/configuration.h"
#include "flagd/sync/v1/sync.grpc.pb.h"
#include "openfeature/evaluation_context.h"

namespace flagd {

class FlagSync {
 public:
  FlagSync();
  virtual ~FlagSync() = default;

  virtual absl::Status Init(const openfeature::EvaluationContext& ctx) = 0;
  virtual absl::Status Shutdown() = 0;

  std::shared_ptr<const nlohmann::json> GetFlags() const;

 protected:
  void UpdateFlags(const nlohmann::json& new_json);

 private:
  mutable std::mutex flags_mutex_;
  std::shared_ptr<const nlohmann::json> current_flags_;
};

class GrpcSync final : public FlagSync {
 public:
  explicit GrpcSync(FlagdProviderConfig config);
  ~GrpcSync() override;

  absl::Status Init(const openfeature::EvaluationContext& ctx) override;
  absl::Status Shutdown() override;

 private:
  enum class State : uint8_t {
    kUninitialized,
    kInitializing,
    kInitialized,
    kShuttingDown,
  };

  void WaitForUpdates();

  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<flagd::sync::v1::FlagSyncService::Stub> stub_;

  std::mutex connection_mutex_;
  std::unique_ptr<grpc::ClientContext> context_;

  std::thread background_thread_;
  std::atomic<bool> shutdown_requested_{false};

  std::mutex state_mutex_;
  std::condition_variable init_cv_;
  State state_ = State::kUninitialized;
  absl::Status init_status_;

  FlagdProviderConfig config_;
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_SYNC_H_