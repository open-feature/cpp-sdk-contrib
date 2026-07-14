#include "providers/flagd/tests/gherkin/test_env.h"

#include <fcntl.h>
#include <signal.h>  // NOLINT(modernize-deprecated-headers) - Need POSIX kill and signals
#include <stdlib.h>  // NOLINT(modernize-deprecated-headers) - Need POSIX setenv
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <sstream>  // NOLINT(misc-include-cleaner) - Used for parsing FLAGD_TEST_FLAGS env var
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "flagd/provider.h"
#include "tools/cpp/runfiles/runfiles.h"

using bazel::tools::cpp::runfiles::Runfiles;
namespace fs = std::filesystem;

namespace openfeature::contrib::flagd::test {

using nlohmann::json;

std::unique_ptr<FlagdProcess> g_flagd;
std::string g_scenario_tmp_dir;
std::shared_ptr<::flagd::FlagdProvider> g_stable_provider;

std::string GetRunfilePath(const std::string& relative_path) {
  static std::unique_ptr<Runfiles> runfiles;
  if (!runfiles) {
    std::string error;
    runfiles.reset(Runfiles::CreateForTest(&error));
    if (!runfiles) {
      std::error_code err_code;
      auto exe_path = fs::canonical("/proc/self/exe", err_code);
      if (!err_code) {
        runfiles.reset(Runfiles::Create(exe_path.string(), &error));
      }
    }
    if (!runfiles) {
      std::cerr << "Failed to create Runfiles: " << error << '\n';
      exit(1);
    }
  }
  std::string path = runfiles->Rlocation(relative_path);
  if (path.empty()) {
    std::cerr << "Failed to resolve runfile: " << relative_path << '\n';
  }
  return path;
}

FlagdProcess::FlagdProcess(std::string binary_path,
                           std::vector<FlagdSource> sources, int port,
                           std::string log_dir)
    : log_dir_(std::move(log_dir)),
      binary_path_(std::move(binary_path)),
      sources_(std::move(sources)),
      port_(port) {}

FlagdProcess::~FlagdProcess() { Stop(); }

std::string FlagdProcess::GetTmpDir() {
  const char* env_tmp = std::getenv("TEST_TMPDIR");
  if (env_tmp) {
    return {env_tmp};
  }
  return ".";
}

bool FlagdProcess::Start() {
  pid_ = fork();
  if (pid_ == -1) {
    std::cerr << "Failed to fork\n";
    return false;
  }

  if (pid_ == 0) {
    std::string tmp_dir = GetTmpDir();
    setenv("HOME", tmp_dir.c_str(), 1);

    std::string log_path = log_dir_ + "/flagd.log";
    int log_fd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd != -1) {
      dup2(log_fd, STDOUT_FILENO);
      dup2(log_fd, STDERR_FILENO);
      close(log_fd);
    }

    json sources_arr = json::array();
    for (const auto& src : sources_) {
      json src_obj = {{"uri", src.path}, {"provider", "file"}};
      if (!src.selector.empty()) {
        src_obj["selector"] = src.selector;
      }
      sources_arr.push_back(src_obj);
    }
    std::string sources_arg = sources_arr.dump();
    std::string port_arg = std::to_string(port_);

    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(binary_path_.c_str()));
    argv.push_back(const_cast<char*>("start"));
    argv.push_back(const_cast<char*>("--sources"));
    argv.push_back(const_cast<char*>(sources_arg.c_str()));
    argv.push_back(const_cast<char*>("--port"));
    argv.push_back(const_cast<char*>(port_arg.c_str()));
    argv.push_back(nullptr);

    execvp(argv[0], argv.data());
    std::cerr << "Failed to exec flagd: " << strerror(errno) << '\n';
    _exit(1);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  return true;
}

void FlagdProcess::Stop() {
  if (pid_ > 0) {
    kill(pid_, SIGTERM);
    int status;
    auto start = std::chrono::steady_clock::now();
    while (waitpid(pid_, &status, WNOHANG) == 0) {
      if (std::chrono::steady_clock::now() - start > std::chrono::seconds(2)) {
        kill(pid_, SIGKILL);
        waitpid(pid_, &status, 0);
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    pid_ = -1;
  }
}

void SetupGlobalFlagd() {
  if (g_flagd) {
    return;
  }

  std::cout << "BEFORE hook: Starting global flagd process\n";

  std::string flagd_bin = GetRunfilePath("flagd_binary/flagd_linux_x86_64");
  if (flagd_bin.empty()) {
    std::cerr << "CRITICAL: Could not find flagd binary in runfiles\n";
    exit(1);
  }

  const char* env_tmp = std::getenv("TEST_TMPDIR");
  fs::path tmp_base = env_tmp ? fs::path(env_tmp) : fs::current_path();
  g_scenario_tmp_dir = (tmp_base / "global_flagd_scenario_dir").string();
  std::cout << "BEFORE hook: Scenario temp dir: " << g_scenario_tmp_dir << '\n';
  fs::create_directories(g_scenario_tmp_dir);

  std::vector<std::string> flags_files;
  if (const char* env_flags = std::getenv("FLAGD_TEST_FLAGS")) {
    std::istringstream iss(env_flags);
    std::string path;
    while (iss >> path) {
      flags_files.push_back(path);
    }
  }

  if (flags_files.empty()) {
    std::string flags_dir = GetRunfilePath("flagd_testbed/flags");
    if (flags_dir.empty() || !fs::exists(flags_dir)) {
      if (fs::exists("../+_repo_rules+flagd_testbed/flags")) {
        flags_dir = "../+_repo_rules+flagd_testbed/flags";
      }
    }
    if (!flags_dir.empty() && fs::exists(flags_dir)) {
      for (const auto& entry : fs::directory_iterator(flags_dir)) {
        if (entry.path().extension() == ".json") {
          flags_files.push_back(entry.path().string());
        }
      }
    }
  }

  json merged_root = json::object();
  merged_root["flags"] = json::object();
  merged_root["metadata"] = json::object();
  merged_root["$evaluators"] = json::object();

  std::vector<FlagdSource> sources;

  for (const auto& flag_file : flags_files) {
    fs::path p(flag_file);
    std::string filename = p.filename().string();

    std::string runfile_path;
    if (fs::exists(flag_file)) {
      runfile_path = flag_file;
    } else {
      runfile_path = GetRunfilePath(flag_file);
      if (runfile_path.empty() &&
          flag_file.find("flagd_testbed/flags/") == std::string::npos) {
        runfile_path = GetRunfilePath("flagd_testbed/flags/" + flag_file);
      }
    }

    if (runfile_path.empty() || !fs::exists(runfile_path)) {
      std::cerr << "WARNING: Could not resolve flag file path: " << flag_file
                << '\n';
      continue;
    }

    if (filename.rfind("selector-", 0) == 0) {
      // Copy selector file to tmp scenario directory
      fs::path dest_selector_path = fs::path(g_scenario_tmp_dir) / filename;
      std::error_code ec;
      fs::copy_file(runfile_path, dest_selector_path,
                    fs::copy_options::overwrite_existing, ec);
      if (ec) {
        std::cerr << "CRITICAL: Could not copy selector file: " << runfile_path
                  << " to " << dest_selector_path << " - " << ec.message()
                  << '\n';
        exit(1);
      }
      // Register selector file source, matching key format
      // "rawflags/selector-flags.json"
      sources.push_back({
          .path = dest_selector_path.string(),
          .selector = "rawflags/" + filename,
      });
      continue;
    }

    std::ifstream ifs(runfile_path);
    if (!ifs.is_open()) {
      std::cerr << "CRITICAL: Could not open flag file: " << runfile_path
                << '\n';
      exit(1);
    }
    json parsed_json = json::parse(ifs, nullptr, false);
    if (!parsed_json.is_discarded() && parsed_json.is_object()) {
      if (parsed_json.contains("flags") && parsed_json["flags"].is_object()) {
        merged_root["flags"].update(parsed_json["flags"]);
      }
      if (parsed_json.contains("metadata") &&
          parsed_json["metadata"].is_object()) {
        merged_root["metadata"].update(parsed_json["metadata"]);
      }
      if (parsed_json.contains("$evaluators") &&
          parsed_json["$evaluators"].is_object()) {
        merged_root["$evaluators"].update(parsed_json["$evaluators"]);
      } else if (parsed_json.contains("evaluators") &&
                 parsed_json["evaluators"].is_object()) {
        merged_root["$evaluators"].update(parsed_json["evaluators"]);
      }
    }
  }

  fs::path dest = fs::path(g_scenario_tmp_dir) / "all_flags.json";
  {
    std::ofstream ofs(dest);
    ofs << merged_root.dump(2);
  }
  // Add all_flags.json as the default (no selector) source
  sources.insert(sources.begin(), {
                                      .path = dest.string(),
                                      .selector = "",
                                  });

  int port = 8013;
  g_flagd = std::make_unique<FlagdProcess>(flagd_bin, sources, port,
                                           g_scenario_tmp_dir);
  if (!g_flagd->Start()) {
    std::cerr << "CRITICAL: Failed to start flagd\n";
    exit(1);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

}  // namespace openfeature::contrib::flagd::test
