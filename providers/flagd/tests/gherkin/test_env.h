#pragma once

#include <sys/types.h>

#include <chrono>
#include <string>
#include <vector>

namespace openfeature::contrib::flagd::test {

// flagd exposes the evaluation API on --port and the flag sync API on
// --sync-port. The provider is in-process only today, so it always talks to
// kFlagdSyncPort; kFlagdRpcPort exists because flagd insists on binding it.
inline constexpr int kFlagdRpcPort = 8013;
inline constexpr int kFlagdSyncPort = 8015;

// Returns an empty string when the path is unknown.
std::string GetRunfilePath(const std::string& relative_path);

bool WaitForGrpcReady(
    const std::string& target,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

std::string FlagdSyncTarget();

struct FlagdSource {
  std::string path;
  std::string selector;
};

// Manages a background Go flagd server subprocess during test execution.
class FlagdProcess {
 public:
  FlagdProcess(std::string binary_path, std::vector<FlagdSource> sources,
               int rpc_port, int sync_port, std::string log_dir);
  ~FlagdProcess();

  FlagdProcess(const FlagdProcess&) = delete;
  FlagdProcess& operator=(const FlagdProcess&) = delete;

  // On failure returns false and fills `error` with the reason, including
  // anything the child managed to report before exec failed.
  bool Start(std::string* error);
  void Stop();

  std::string LogPath() const;
  std::string TailLog(int max_lines = 40) const;

 private:
  std::string binary_path_;
  std::vector<FlagdSource> sources_;
  int rpc_port_;
  int sync_port_;
  std::string log_dir_;
  pid_t pid_ = -1;
};

// Writes the merged fixture files and starts the shared flagd instance. Safe
// to call more than once; only the first call does anything.
bool SetupGlobalFlagd(std::string* error);

// Safe to call when flagd was never started.
void TeardownGlobalFlagd();

std::string GlobalFlagdLogTail();

}  // namespace openfeature::contrib::flagd::test
