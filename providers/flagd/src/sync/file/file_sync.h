#ifndef CPP_SDK_FLAGD_SYNC_FILE_SYNC_H_
#define CPP_SDK_FLAGD_SYNC_FILE_SYNC_H_

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "absl/status/status.h"
#include "flagd/configuration.h"
#include "flagd/sync/sync.h"
#include "openfeature/evaluation_context.h"

namespace flagd {

class FileSync final : public FlagSync {
 public:
  explicit FileSync(FlagdProviderConfig config);
  ~FileSync() override;

  absl::Status Init(const openfeature::EvaluationContext& ctx) override;
  absl::Status Shutdown() override;

 private:
  absl::Status ReadAndUpdateFlags();
  void PollForChanges();

  FlagdProviderConfig config_;
  std::string source_path_;
  int poll_interval_ms_;

  std::thread poll_thread_;

  std::mutex lifecycle_mutex_;
  std::condition_variable shutdown_cv_;
  bool shutdown_requested_ = false;
  bool initialized_ = false;
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_SYNC_FILE_SYNC_H_
