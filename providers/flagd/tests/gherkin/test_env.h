#pragma once

#include <memory>
#include <string>
#include <vector>

#include "flagd/provider.h"

namespace openfeature::contrib::flagd::test {

// Helper to resolve Bazel runfiles for test fixtures and binaries.
std::string GetRunfilePath(const std::string& relative_path);

// Manages a background Go flagd server subprocess during test execution.
class FlagdProcess {
 public:
  FlagdProcess(std::string binary_path, std::vector<std::string> config_paths,
               int port, std::string log_dir);
  ~FlagdProcess();

  bool Start();
  void Stop();

 private:
  std::string GetTmpDir();

  std::string log_dir_;
  std::string binary_path_;
  std::vector<std::string> config_paths_;
  int port_;
  pid_t pid_ = -1;
};

extern std::unique_ptr<FlagdProcess> g_flagd;
extern std::string g_scenario_tmp_dir;
extern std::shared_ptr<::flagd::FlagdProvider> g_stable_provider;

// Initializes the global flagd test process and merges JSON test fixtures.
void SetupGlobalFlagd();

}  // namespace openfeature::contrib::flagd::test
