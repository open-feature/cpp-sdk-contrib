#ifndef PROVIDERS_FLAGD_TESTS_GHERKIN_TEST_ENV_H_
#define PROVIDERS_FLAGD_TESTS_GHERKIN_TEST_ENV_H_

#include <sys/types.h>

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace openfeature::contrib::flagd::test {

// flagd exposes the evaluation API on --port and the flag sync API on
// --sync-port. The provider is in-process only today, so it always talks to
// kFlagdSyncPort; kFlagdRpcPort exists because flagd insists on binding it.
inline constexpr int kFlagdRpcPort = 8013;
inline constexpr int kFlagdSyncPort = 8015;

absl::StatusOr<std::string> GetRunfilePath(std::string_view relative_path);

// Resolves a feature file CLI argument: returns `feature_arg` unchanged if it
// already exists on disk, or resolves it against @flagd_testbed runfiles when a
// bare filename (e.g. "targeting.feature") or runfiles-relative path is given.
std::string ResolveFeaturePath(std::string_view feature_arg);

bool WaitForGrpcReady(
    const std::string& target,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

std::string FlagdSyncTarget();

struct FlagdSource {
  // Relative to FlagdProcess's working directory (`work_dir`). For `"file"`
  // sync providers, flagd ignores `"selector"` in `--sources` and indexes flags
  // by `source = uri`, so this URI must match the selector strings used by the
  // Gherkin feature files (e.g. "flags/allFlags.json",
  // "rawflags/selector-flags.json").
  std::string uri;
};

// Manages a background Go flagd server subprocess during test execution.
class FlagdProcess {
 public:
  FlagdProcess(std::string binary_path, std::vector<FlagdSource> sources,
               int rpc_port, int sync_port, std::string work_dir);
  ~FlagdProcess();

  FlagdProcess(const FlagdProcess&) = delete;
  FlagdProcess& operator=(const FlagdProcess&) = delete;

  // A failed status carries anything the child managed to report before exec
  // failed.
  absl::Status Start();
  void Stop();

  // Returns true if the child process is still running (has not exited).
  bool IsRunning() const;

  std::string LogPath() const;
  std::string TailLog(int max_lines = 40) const;

 private:
  std::string binary_path_;
  std::vector<FlagdSource> sources_;
  int rpc_port_;
  int sync_port_;
  std::string work_dir_;
  pid_t pid_ = -1;
};

// Writes the merged fixture files and starts the shared flagd instance. Safe
// to call more than once; only the first call does anything.
absl::Status SetupGlobalFlagd();

// Safe to call when flagd was never started.
void TeardownGlobalFlagd();

std::string GlobalFlagdLogTail();

}  // namespace openfeature::contrib::flagd::test

#endif  // PROVIDERS_FLAGD_TESTS_GHERKIN_TEST_ENV_H_
