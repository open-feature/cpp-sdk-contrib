#include "file_sync.h"

#include <chrono>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

using Json = nlohmann::json;

namespace flagd {

FileSync::FileSync(FlagdProviderConfig config)
    : config_(std::move(config)),
      source_path_(*config_.GetOfflineFlagSourcePath()),
      poll_interval_ms_(config_.GetOfflinePollIntervalMs()) {}

FileSync::~FileSync() {
  absl::Status status = FileSync::Shutdown();
  if (!status.ok()) {
    LOG(ERROR) << "Error during FileSync shutdown: " << status;
  }
}

absl::Status FileSync::ReadAndUpdateFlags() {
  std::ifstream file(source_path_);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("Unable to open flag source file: ", source_path_));
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  Json parsed;
  try {
    parsed = Json::parse(buffer.str());
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to parse flag source file: ", e.what()));
  }

  UpdateFlags(parsed);
  return absl::OkStatus();
}

void FileSync::PollForChanges() {
  while (true) {
    std::unique_lock lock(lifecycle_mutex_);
    if (shutdown_cv_.wait_for(
            lock, std::chrono::milliseconds(poll_interval_ms_),
            [this] { return shutdown_requested_; })) {
      return;
    }
    lock.unlock();

    absl::Status status = ReadAndUpdateFlags();
    if (!status.ok()) {
      LOG(ERROR) << "Failed to read flag source file: " << status;
    }
  }
}

absl::Status FileSync::Init(const openfeature::EvaluationContext& ctx) {
  std::unique_lock lock(lifecycle_mutex_);

  if (initialized_) {
    return absl::OkStatus();
  }

  lock.unlock();
  absl::Status status = ReadAndUpdateFlags();
  if (!status.ok()) {
    return status;
  }
  lock.lock();

  try {
    poll_thread_ = std::thread(&FileSync::PollForChanges, this);
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrCat("Failed to spawn poll thread: ", e.what()));
  }

  initialized_ = true;
  return absl::OkStatus();
}

absl::Status FileSync::Shutdown() {
  std::unique_lock lock(lifecycle_mutex_);

  if (!initialized_) {
    return absl::OkStatus();
  }

  shutdown_requested_ = true;
  initialized_ = false;
  lock.unlock();

  shutdown_cv_.notify_all();
  if (poll_thread_.joinable()) {
    poll_thread_.join();
  }

  return absl::OkStatus();
}

}  // namespace flagd
