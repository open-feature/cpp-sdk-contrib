#include "providers/flagd/tests/gherkin/test_env.h"

#include <fcntl.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "tools/cpp/runfiles/runfiles.h"

using bazel::tools::cpp::runfiles::Runfiles;
namespace fs = std::filesystem;

namespace openfeature::contrib::flagd::test {

using nlohmann::json;

namespace {

// Fixture files that cannot be loaded yet. flagd's FlagSync rejects the whole
// payload when schema validation fails, and these two deliberately contain
// targeting rules that are only supposed to be caught at evaluation time.
// TODO(#129): re-enable once FlagSync tolerates them.
constexpr std::string_view kUnsupportedFixtures[] = {"edge-case-flags.json",
                                                     "custom-ops.json"};

std::unique_ptr<FlagdProcess> g_flagd;
std::string g_scenario_tmp_dir;

bool IsUnsupportedFixture(std::string_view filename) {
  for (std::string_view unsupported : kUnsupportedFixtures) {
    if (filename == unsupported) {
      return true;
    }
  }
  return false;
}

std::string TmpBaseDir() {
  const char* env_tmp = getenv("TEST_TMPDIR");
  if (env_tmp != nullptr && *env_tmp != '\0') {
    return env_tmp;
  }
  return fs::current_path().string();
}

// Last write wins, but collisions are reported rather than silent.
void MergeObject(json& target, const json& source, std::string_view section,
                 std::string_view origin) {
  for (const auto& [key, value] : source.items()) {
    if (target.contains(key)) {
      std::cerr << "WARNING: duplicate " << section << " key '" << key
                << "' redefined by " << origin
                << "; the later definition wins\n";
    }
    target[key] = value;
  }
}

std::vector<std::string> CollectFixtureFiles() {
  std::vector<std::string> files;
  const char* env_flags = getenv("FLAGD_TEST_FLAGS");
  if (env_flags != nullptr) {
    files =
        absl::StrSplit(env_flags, absl::ByAnyChar(" \t\n"), absl::SkipEmpty());
  }
  if (!files.empty()) {
    return files;
  }

  const absl::StatusOr<std::string> flags_dir =
      GetRunfilePath("flagd_testbed/flags");
  if (!flags_dir.ok() || !fs::exists(*flags_dir)) {
    return files;
  }
  for (const auto& entry : fs::directory_iterator(*flags_dir)) {
    if (entry.path().extension() == ".json") {
      files.push_back(entry.path().string());
    }
  }
  return files;
}

// A fixture path may be absolute, runfiles-relative, or a bare filename inside
// the testbed's flags directory.
absl::StatusOr<std::string> ResolveFixturePath(std::string_view fixture) {
  if (fs::exists(fixture)) {
    return std::string(fixture);
  }
  absl::StatusOr<std::string> resolved = GetRunfilePath(fixture);
  if (resolved.ok() && fs::exists(*resolved)) {
    return *resolved;
  }
  if (fixture.find("flagd_testbed/flags/") == std::string_view::npos) {
    resolved = GetRunfilePath(absl::StrCat("flagd_testbed/flags/", fixture));
    if (resolved.ok() && fs::exists(*resolved)) {
      return *resolved;
    }
  }
  return absl::NotFoundError(
      absl::StrCat("could not resolve fixture path: ", fixture));
}

absl::StatusOr<std::vector<FlagdSource>> StageTestbedFixtures(
    const fs::path& work_dir) {
  const std::vector<std::string> fixtures = CollectFixtureFiles();
  if (fixtures.empty()) {
    return absl::FailedPreconditionError(
        "no flag fixtures found; expected FLAGD_TEST_FLAGS to be set by the "
        "Bazel target, or flagd_testbed/flags to be present in runfiles");
  }

  std::error_code ec;
  const fs::path flags_dir = work_dir / "flags";
  const fs::path rawflags_dir = work_dir / "rawflags";
  fs::create_directories(flags_dir, ec);
  if (ec) {
    return absl::InternalError(absl::StrCat(
        "could not create ", flags_dir.string(), ": ", ec.message()));
  }
  fs::create_directories(rawflags_dir, ec);
  if (ec) {
    return absl::InternalError(absl::StrCat(
        "could not create ", rawflags_dir.string(), ": ", ec.message()));
  }

  json merged = json::object();
  merged["flags"] = json::object();
  merged["metadata"] = json::object();
  merged["$evaluators"] = json::object();

  std::vector<FlagdSource> sources;
  int merged_count = 0;

  for (const std::string& fixture : fixtures) {
    const std::string filename = fs::path(fixture).filename().string();
    if (IsUnsupportedFixture(filename)) {
      std::cerr << "NOTE: skipping fixture " << filename
                << " (see TODO(#129)); the scenarios that depend on it will "
                   "fail\n";
      continue;
    }

    const absl::StatusOr<std::string> path = ResolveFixturePath(fixture);
    if (!path.ok()) {
      return path.status();
    }

    // Files named selector-*.json back the selector scenarios, which need each
    // file to stay an addressable sync source of its own rather than being
    // folded into the combined payload. Stage them under `rawflags/<filename>`
    // so flagd registers their file URI as `"rawflags/<filename>"`.
    if (filename.rfind("selector-", 0) == 0) {
      const fs::path dest = rawflags_dir / filename;
      fs::copy_file(*path, dest, fs::copy_options::overwrite_existing, ec);
      if (ec) {
        return absl::InternalError(
            absl::StrCat("could not copy selector fixture ", *path, " to ",
                         dest.string(), ": ", ec.message()));
      }
      sources.push_back({.uri = absl::StrCat("rawflags/", filename)});
      continue;
    }

    std::ifstream ifs(*path);
    if (!ifs.is_open()) {
      return absl::InternalError(
          absl::StrCat("could not open fixture ", *path));
    }
    json parsed = json::parse(ifs, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
      return absl::InvalidArgumentError(
          absl::StrCat("fixture ", *path, " is not a JSON object"));
    }

    if (parsed.contains("flags") && parsed["flags"].is_object()) {
      MergeObject(merged["flags"], parsed["flags"], "flag", filename);
    }
    if (parsed.contains("metadata") && parsed["metadata"].is_object()) {
      // Flag-set metadata is really a per-source concept. Collapsing every
      // fixture into one source means the scenarios see a union no real
      // deployment would produce, so at least make collisions visible.
      // TODO(#129): register each fixture as its own flagd sync source.
      MergeObject(merged["metadata"], parsed["metadata"], "flag-set metadata",
                  filename);
    }
    const char* evaluators_key = parsed.contains("$evaluators")  ? "$evaluators"
                                 : parsed.contains("evaluators") ? "evaluators"
                                                                 : nullptr;
    if (evaluators_key != nullptr && parsed[evaluators_key].is_object()) {
      MergeObject(merged["$evaluators"], parsed[evaluators_key], "$evaluator",
                  filename);
    }
    ++merged_count;
  }

  if (merged_count == 0) {
    return absl::FailedPreconditionError(
        "every fixture was skipped; nothing to serve");
  }

  const fs::path combined = flags_dir / "allFlags.json";
  {
    std::ofstream ofs(combined);
    if (!ofs.is_open()) {
      return absl::InternalError(
          absl::StrCat("could not write ", combined.string()));
    }
    ofs << merged.dump(2);
    if (!ofs.good()) {
      return absl::InternalError(
          absl::StrCat("failed while writing ", combined.string()));
    }
  }
  sources.insert(sources.begin(), {.uri = "flags/allFlags.json"});
  return sources;
}

bool WaitForFlagdProcessReady(const FlagdProcess& process,
                              const std::string& target,
                              std::chrono::milliseconds timeout) {
  auto channel =
      grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
  const auto deadline = std::chrono::steady_clock::now() + timeout;

  while (std::chrono::steady_clock::now() < deadline) {
    if (!process.IsRunning()) {
      return false;
    }
    const auto slice = std::min<std::chrono::milliseconds>(
        std::chrono::milliseconds(50),
        std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()));
    if (channel->WaitForConnected(std::chrono::system_clock::now() + slice)) {
      // Confirm our own child is still alive so we do not mistake an
      // already-running flagd on the same port for a healthy startup.
      return process.IsRunning();
    }
  }
  return false;
}

}  // namespace

absl::StatusOr<std::string> GetRunfilePath(std::string_view relative_path) {
  static std::string* creation_error = new std::string();
  static Runfiles* runfiles = [] {
    std::string error;
    Runfiles* created = Runfiles::CreateForTest(&error);
    if (created == nullptr) {
      std::error_code err_code;
      const auto exe_path = fs::canonical("/proc/self/exe", err_code);
      if (!err_code) {
        created = Runfiles::Create(exe_path.string(), &error);
      }
    }
    if (created == nullptr) {
      *creation_error = error;
    }
    return created;
  }();

  if (runfiles == nullptr) {
    return absl::InternalError(
        absl::StrCat("failed to create Runfiles: ", *creation_error));
  }
  std::string resolved = runfiles->Rlocation(std::string(relative_path));
  if (resolved.empty()) {
    return absl::NotFoundError(
        absl::StrCat("no runfile named '", relative_path, "'"));
  }
  return resolved;
}

std::string ResolveFeaturePath(std::string_view feature_arg) {
  if (fs::exists(feature_arg)) {
    return std::string(feature_arg);
  }
  if (const auto resolved = GetRunfilePath(feature_arg);
      resolved.ok() && fs::exists(*resolved)) {
    return *resolved;
  }
  if (const auto resolved =
          GetRunfilePath(absl::StrCat("flagd_testbed/", feature_arg));
      resolved.ok() && fs::exists(*resolved)) {
    return *resolved;
  }
  if (const auto resolved =
          GetRunfilePath(absl::StrCat("flagd_testbed/gherkin/", feature_arg));
      resolved.ok() && fs::exists(*resolved)) {
    return *resolved;
  }
  return std::string(feature_arg);
}

bool WaitForGrpcReady(const std::string& target,
                      std::chrono::milliseconds timeout) {
  auto channel =
      grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
  return channel->WaitForConnected(std::chrono::system_clock::now() + timeout);
}

std::string FlagdSyncTarget() {
  return absl::StrCat("localhost:", kFlagdSyncPort);
}

FlagdProcess::FlagdProcess(std::string binary_path,
                           std::vector<FlagdSource> sources, int rpc_port,
                           int sync_port, std::string work_dir)
    : binary_path_(std::move(binary_path)),
      sources_(std::move(sources)),
      rpc_port_(rpc_port),
      sync_port_(sync_port),
      work_dir_(std::move(work_dir)) {}

FlagdProcess::~FlagdProcess() { Stop(); }

bool FlagdProcess::IsRunning() const {
  if (pid_ <= 0) {
    return false;
  }
  int status = 0;
  return waitpid(pid_, &status, WNOHANG) == 0;
}

std::string FlagdProcess::LogPath() const {
  return absl::StrCat(work_dir_, "/flagd.log");
}

std::string FlagdProcess::TailLog(int max_lines) const {
  std::ifstream ifs(LogPath());
  if (!ifs.is_open()) {
    return absl::StrCat("(no flagd log at ", LogPath(), ")");
  }
  std::deque<std::string> lines;
  std::string line;
  while (std::getline(ifs, line)) {
    lines.push_back(line);
    if (static_cast<int>(lines.size()) > max_lines) {
      lines.pop_front();
    }
  }
  std::string result;
  for (const std::string& kept : lines) {
    absl::StrAppend(&result, "  | ", kept, "\n");
  }
  return result.empty() ? "(flagd log is empty)" : result;
}

absl::Status FlagdProcess::Start() {
  // Built in the parent: between fork() and execvp() only async-signal-safe
  // calls are legal, and allocating (as std::string and nlohmann::json do) can
  // deadlock on a malloc lock another thread held at the moment of the fork.
  json sources_arr = json::array();
  for (const auto& src : sources_) {
    sources_arr.push_back({{"uri", src.uri}, {"provider", "file"}});
  }
  const std::string sources_arg = sources_arr.dump();
  const std::string rpc_port_arg = std::to_string(rpc_port_);
  const std::string sync_port_arg = std::to_string(sync_port_);
  const std::string log_path = LogPath();
  const std::string home_dir = TmpBaseDir();

  std::vector<char*> argv = {
      binary_path_.data(),
      const_cast<char*>("start"),
      const_cast<char*>("--sources"),
      const_cast<char*>(sources_arg.c_str()),
      const_cast<char*>("--port"),
      const_cast<char*>(rpc_port_arg.c_str()),
      const_cast<char*>("--sync-port"),
      const_cast<char*>(sync_port_arg.c_str()),
      nullptr,
  };

  // Lets the child report an execvp failure instead of the parent having to
  // infer it from a readiness timeout several seconds later.
  int exec_status[2];
  if (pipe(exec_status) != 0) {
    return absl::InternalError(
        absl::StrCat("pipe() failed: ", strerror(errno)));
  }
  if (fcntl(exec_status[1], F_SETFD, FD_CLOEXEC) != 0) {
    close(exec_status[0]);
    close(exec_status[1]);
    return absl::InternalError(
        absl::StrCat("fcntl(FD_CLOEXEC) failed: ", strerror(errno)));
  }

  const pid_t parent_pid = getpid();
  pid_ = fork();
  if (pid_ == -1) {
    close(exec_status[0]);
    close(exec_status[1]);
    return absl::InternalError(
        absl::StrCat("fork() failed: ", strerror(errno)));
  }

  if (pid_ == 0) {
    close(exec_status[0]);

    // Terminate if the parent test runner exits or crashes. Compare against
    // `parent_pid` rather than 1 because under a subreaper (e.g. Bazel's
    // linux-sandbox or systemd --user), an orphaned child is reparented to the
    // subreaper PID rather than init.
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    if (getppid() != parent_pid) {
      _exit(1);
    }

    setenv("HOME", home_dir.c_str(), 1);

    const int log_fd =
        open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd != -1) {
      dup2(log_fd, STDOUT_FILENO);
      dup2(log_fd, STDERR_FILENO);
      if (log_fd > STDERR_FILENO) {
        close(log_fd);
      }
    }

    if (chdir(work_dir_.c_str()) != 0) {
      const int chdir_errno = errno;
      ssize_t ignored =
          write(exec_status[1], &chdir_errno, sizeof(chdir_errno));
      static_cast<void>(ignored);
      _exit(127);
    }

    execvp(argv[0], argv.data());

    const int exec_errno = errno;
    ssize_t ignored = write(exec_status[1], &exec_errno, sizeof(exec_errno));
    static_cast<void>(ignored);
    _exit(127);
  }

  close(exec_status[1]);
  int child_errno = 0;
  const ssize_t got = read(exec_status[0], &child_errno, sizeof(child_errno));
  close(exec_status[0]);

  if (got == static_cast<ssize_t>(sizeof(child_errno))) {
    int status = 0;
    waitpid(pid_, &status, 0);
    pid_ = -1;
    return absl::InternalError(absl::StrCat("failed to exec '", binary_path_,
                                            "': ", strerror(child_errno)));
  }
  return absl::OkStatus();
}

void FlagdProcess::Stop() {
  if (pid_ <= 0) {
    return;
  }
  kill(pid_, SIGTERM);
  int status = 0;
  const auto start = std::chrono::steady_clock::now();
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

absl::Status SetupGlobalFlagd() {
  if (g_flagd) {
    return absl::OkStatus();
  }

  const absl::StatusOr<std::string> flagd_bin =
      GetRunfilePath("flagd_binary/flagd_linux_x86_64");
  if (!flagd_bin.ok()) {
    return absl::NotFoundError(
        absl::StrCat("could not find the flagd binary in runfiles: ",
                     flagd_bin.status().message()));
  }

  g_scenario_tmp_dir = (fs::path(TmpBaseDir()) / "gherkin_flagd").string();
  std::error_code ec;
  fs::create_directories(g_scenario_tmp_dir, ec);
  if (ec) {
    return absl::InternalError(absl::StrCat(
        "could not create ", g_scenario_tmp_dir, ": ", ec.message()));
  }

  absl::StatusOr<std::vector<FlagdSource>> sources =
      StageTestbedFixtures(g_scenario_tmp_dir);
  if (!sources.ok()) {
    return sources.status();
  }

  g_flagd = std::make_unique<FlagdProcess>(*flagd_bin, *std::move(sources),
                                           kFlagdRpcPort, kFlagdSyncPort,
                                           g_scenario_tmp_dir);
  if (const absl::Status started = g_flagd->Start(); !started.ok()) {
    g_flagd.reset();
    return absl::InternalError(
        absl::StrCat("could not start flagd: ", started.message()));
  }

  if (!WaitForFlagdProcessReady(*g_flagd, FlagdSyncTarget(),
                                std::chrono::milliseconds(5000))) {
    const std::string log_path = g_flagd->LogPath();
    const std::string log_tail = g_flagd->TailLog();
    g_flagd.reset();
    return absl::UnavailableError(
        absl::StrCat("flagd did not become ready on ", FlagdSyncTarget(),
                     "\nlast lines of ", log_path, ":\n", log_tail));
  }
  return absl::OkStatus();
}

void TeardownGlobalFlagd() { g_flagd.reset(); }

std::string GlobalFlagdLogTail() {
  return g_flagd ? g_flagd->TailLog() : "(flagd was never started)";
}

}  // namespace openfeature::contrib::flagd::test
