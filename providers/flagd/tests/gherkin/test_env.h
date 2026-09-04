#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "flagd/provider.h"

namespace openfeature::contrib::flagd::test {

// Helper to resolve Bazel runfiles for test fixtures and binaries.
std::string GetRunfilePath(const std::string& relative_path);

// Polls the gRPC channel connection state until it reaches GRPC_CHANNEL_READY
// or times out.
bool WaitForGrpcReady(
    const std::string& target,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

struct FlagdSource {
  std::string path;
  std::string selector;
};

// Manages a background Go flagd server subprocess during test execution.
class FlagdProcess {
 public:
  FlagdProcess(std::string binary_path, std::vector<FlagdSource> sources,
               int port, std::string log_dir);
  ~FlagdProcess();

  bool Start();
  void Stop();
  bool IsAlive() const;

 private:
  std::string GetTmpDir();

  std::string log_dir_;
  std::string binary_path_;
  std::vector<FlagdSource> sources_;
  int port_;
  pid_t pid_ = -1;
};

extern std::unique_ptr<FlagdProcess> g_flagd;
extern std::string g_scenario_tmp_dir;
extern std::shared_ptr<::flagd::FlagdProvider> g_stable_provider;

// Initializes the global flagd test process and merges JSON test fixtures.
void SetupGlobalFlagd();

}  // namespace openfeature::contrib::flagd::test
