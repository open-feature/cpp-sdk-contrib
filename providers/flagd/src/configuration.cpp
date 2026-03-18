#include "configuration.h"

#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "nlohmann/json.hpp"

namespace flagd {

namespace {
constexpr double kMsInSecond = 1000.0;

struct EnvVars {
  static constexpr std::string_view kHost = "FLAGD_HOST";
  static constexpr std::string_view kPort = "FLAGD_PORT";
  static constexpr std::string_view kTargetUri = "FLAGD_TARGET_URI";
  static constexpr std::string_view kTls = "FLAGD_TLS";
  static constexpr std::string_view kSocketPath = "FLAGD_SOCKET_PATH";
  static constexpr std::string_view kServerCertPath = "FLAGD_SERVER_CERT_PATH";
  static constexpr std::string_view kDeadlineMs = "FLAGD_DEADLINE_MS";
  static constexpr std::string_view kStreamDeadlineMs =
      "FLAGD_STREAM_DEADLINE_MS";
  static constexpr std::string_view kRetryBackoffMs = "FLAGD_RETRY_BACKOFF_MS";
  static constexpr std::string_view kRetryBackoffMaxMs =
      "FLAGD_RETRY_BACKOFF_MAX_MS";
  static constexpr std::string_view kRetryGracePeriod =
      "FLAGD_RETRY_GRACE_PERIOD";
  static constexpr std::string_view kKeepAliveTimeMs =
      "FLAGD_KEEP_ALIVE_TIME_MS";
  static constexpr std::string_view kFatalStatusCodes =
      "FLAGD_FATAL_STATUS_CODES";
  static constexpr std::string_view kSourceSelector = "FLAGD_SOURCE_SELECTOR";
  static constexpr std::string_view kProviderId = "FLAGD_PROVIDER_ID";
  static constexpr std::string_view kOfflineFlagSourcePath =
      "FLAGD_OFFLINE_FLAG_SOURCE_PATH";
  static constexpr std::string_view kOfflinePollMs = "FLAGD_OFFLINE_POLL_MS";
};

struct Defaults {
  static constexpr std::string_view kHost = "localhost";
  static constexpr int kPortInProcess = 8015;
  static constexpr bool kTls = false;
  static constexpr int kDeadlineMs = 500;
  static constexpr int kStreamDeadlineMs = 600000;
  static constexpr int kRetryBackoffMs = 1000;
  static constexpr int kRetryBackoffMaxMs = 12000;
  static constexpr int kRetryGracePeriod = 5;
  static constexpr int kKeepAliveTimeMs = 0;
  static constexpr int kOfflinePollMs = 5000;
};

const std::map<std::string, int> kStatusCodeMap = {
    {"OK", 0},
    {"CANCELLED", 1},
    {"UNKNOWN", 2},
    {"INVALID_ARGUMENT", 3},
    {"DEADLINE_EXCEEDED", 4},
    {"NOT_FOUND", 5},
    {"ALREADY_EXISTS", 6},
    {"PERMISSION_DENIED", 7},
    {"UNAUTHENTICATED", 16},
    {"RESOURCE_EXHAUSTED", 8},
    {"FAILED_PRECONDITION", 9},
    {"ABORTED", 10},
    {"OUT_OF_RANGE", 11},
    {"UNIMPLEMENTED", 12},
    {"INTERNAL", 13},
    {"UNAVAILABLE", 14},
    {"DATA_LOSS", 15},
};
}  // namespace

// --- Helpers ---
static std::string GetEnvStr(const std::string_view name,
                             const std::string_view default_value = "") {
  const char* val = std::getenv(std::string(name).c_str());
  return (val != nullptr) ? std::string(val) : std::string(default_value);
}

static int GetEnvInt(const std::string_view name, int default_value) {
  const char* val = std::getenv(std::string(name).c_str());
  if (val != nullptr) {
    try {
      return std::stoi(val);
    } catch (...) {
      return default_value;
    }
  }
  return default_value;
}

static bool GetEnvBool(const std::string_view name, bool default_value) {
  const char* val = std::getenv(std::string(name).c_str());
  if (val == nullptr) {
    return default_value;
  }
  std::string str(val);
  std::transform(str.begin(), str.end(), str.begin(),
                 [](unsigned char chr) { return std::tolower(chr); });
  return str == "true" || str == "1";
}

static std::vector<int> ParseFatalStatusCodes(const std::string& str) {
  std::vector<int> result;
  std::stringstream sstream(str);
  std::string item;
  while (std::getline(sstream, item, ',')) {
    item.erase(0, item.find_first_not_of(" \t\n\r"));
    item.erase(item.find_last_not_of(" \t\n\r") + 1);

    if (item.empty()) continue;

    try {
      int code = std::stoi(item);
      result.push_back(code);
      continue;
    } catch (const std::invalid_argument&) {
      // Not an integer, try parsing as a status code string.
    } catch (const std::out_of_range&) {
      // Not a valid integer, try parsing as a status code string.
    }

    auto iter = kStatusCodeMap.find(item);
    if (iter != kStatusCodeMap.end()) {
      result.push_back(iter->second);
    } else {
      LOG(WARNING) << "Unknown gRPC status code: " << item;
    }
  }
  return result;
}

FlagdProviderConfig::FlagdProviderConfig()
    : host_(GetEnvStr(EnvVars::kHost, Defaults::kHost)),
      port_(GetEnvInt(EnvVars::kPort, Defaults::kPortInProcess)),
      tls_(GetEnvBool(EnvVars::kTls, Defaults::kTls)),
      deadline_ms_(GetEnvInt(EnvVars::kDeadlineMs, Defaults::kDeadlineMs)),
      stream_deadline_ms_(
          GetEnvInt(EnvVars::kStreamDeadlineMs, Defaults::kStreamDeadlineMs)),
      retry_backoff_ms_(
          GetEnvInt(EnvVars::kRetryBackoffMs, Defaults::kRetryBackoffMs)),
      retry_backoff_max_ms_(
          GetEnvInt(EnvVars::kRetryBackoffMaxMs, Defaults::kRetryBackoffMaxMs)),
      retry_grace_period_(
          GetEnvInt(EnvVars::kRetryGracePeriod, Defaults::kRetryGracePeriod)),
      keep_alive_time_ms_(
          GetEnvInt(EnvVars::kKeepAliveTimeMs, Defaults::kKeepAliveTimeMs)),
      offline_poll_interval_ms_(
          GetEnvInt(EnvVars::kOfflinePollMs, Defaults::kOfflinePollMs)) {
  if (std::string val = GetEnvStr(EnvVars::kTargetUri); !val.empty()) {
    target_uri_ = val;
  }
  if (std::string val = GetEnvStr(EnvVars::kSocketPath); !val.empty()) {
    socket_path_ = val;
  }
  if (std::string val = GetEnvStr(EnvVars::kServerCertPath); !val.empty()) {
    cert_path_ = val;
  }
  if (std::string val = GetEnvStr(EnvVars::kSourceSelector); !val.empty()) {
    selector_ = val;
  }
  if (std::string val = GetEnvStr(EnvVars::kProviderId); !val.empty()) {
    provider_id_ = val;
  }
  if (std::string val = GetEnvStr(EnvVars::kOfflineFlagSourcePath);
      !val.empty()) {
    offline_flag_source_path_ = val;
  }
  if (std::string val = GetEnvStr(EnvVars::kFatalStatusCodes); !val.empty()) {
    fatal_status_codes_ = ParseFatalStatusCodes(val);
  }
}

std::string FlagdProviderConfig::GetEffectiveTargetUri() const {
  if (target_uri_.has_value() && !target_uri_->empty()) return *target_uri_;
  if (socket_path_.has_value() && !socket_path_->empty()) {
    return absl::StrCat("unix://", *socket_path_);
  }
  return absl::StrCat(host_, ":", std::to_string(port_));
}

absl::StatusOr<std::shared_ptr<grpc::ChannelCredentials>>
FlagdProviderConfig::GetEffectiveCredentials() const {
  if (channel_credentials_) {
    return channel_credentials_;
  }

  if (tls_) {
    grpc::SslCredentialsOptions ssl_opts;
    // If we don't provide ssl_opts.pem_root_certs, grpc will use defaults.
    // https://grpc.github.io/grpc/cpp/structgrpc_1_1_ssl_credentials_options.html
    if (cert_path_.has_value() && !cert_path_->empty()) {
      std::ifstream file(*cert_path_);
      if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        ssl_opts.pem_root_certs = buffer.str();
      } else {
        return absl::InternalError("Failed to open certificate file: " +
                                   *cert_path_);
      }
    }
    return grpc::SslCredentials(ssl_opts);
  }

  return grpc::InsecureChannelCredentials();
}

// --- Getters ---
const std::string& FlagdProviderConfig::GetHost() const { return host_; }
int FlagdProviderConfig::GetPort() const { return port_; }
std::optional<std::string> FlagdProviderConfig::GetTargetUri() const {
  return target_uri_;
}
bool FlagdProviderConfig::GetTls() const { return tls_; }
std::shared_ptr<grpc::ChannelCredentials>
FlagdProviderConfig::GetChannelCredentials() const {
  return channel_credentials_;
}
std::optional<std::string> FlagdProviderConfig::GetSocketPath() const {
  return socket_path_;
}
std::optional<std::string> FlagdProviderConfig::GetCertPath() const {
  return cert_path_;
}
int FlagdProviderConfig::GetDeadlineMs() const { return deadline_ms_; }
int FlagdProviderConfig::GetStreamDeadlineMs() const {
  return stream_deadline_ms_;
}
int FlagdProviderConfig::GetRetryBackoffMs() const { return retry_backoff_ms_; }
int FlagdProviderConfig::GetRetryBackoffMaxMs() const {
  return retry_backoff_max_ms_;
}
int FlagdProviderConfig::GetRetryGracePeriod() const {
  return retry_grace_period_;
}
int FlagdProviderConfig::GetKeepAliveTimeMs() const {
  return keep_alive_time_ms_;
}
const std::vector<int>& FlagdProviderConfig::GetFatalStatusCodes() const {
  return fatal_status_codes_;
}

std::string FlagdProviderConfig::GetServiceConfigJson() const {
  const auto names = nlohmann::json::array({
      nlohmann::json::object({{"service", "flagd.evaluation.v1.Service"}}),
      nlohmann::json::object({{"service", "flagd.sync.v1.FlagSyncService"}}),
  });

  const auto retry_policy = nlohmann::json::object({
      {"maxAttempts", 4},
      {"initialBackoff", absl::StrCat(retry_backoff_ms_ / kMsInSecond, "s")},
      {"maxBackoff", absl::StrCat(retry_backoff_max_ms_ / kMsInSecond, "s")},
      {"backoffMultiplier", 2},
      {
          "retryableStatusCodes",
          nlohmann::json::array({"UNAVAILABLE", "UNKNOWN"}),
      },
  });

  const auto method_config = nlohmann::json::object({
      {"name", names},
      {"retryPolicy", retry_policy},
  });

  return nlohmann::json::object(
             {{"methodConfig", nlohmann::json::array({method_config})}})
      .dump();
}

std::optional<std::string> FlagdProviderConfig::GetSelector() const {
  return selector_;
}
std::optional<std::string> FlagdProviderConfig::GetProviderId() const {
  return provider_id_;
}
std::optional<std::string> FlagdProviderConfig::GetOfflineFlagSourcePath()
    const {
  return offline_flag_source_path_;
}
int FlagdProviderConfig::GetOfflinePollIntervalMs() const {
  return offline_poll_interval_ms_;
}

// --- Setters ---
FlagdProviderConfig& FlagdProviderConfig::SetHost(std::string_view host) {
  host_ = host;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetPort(int port) {
  port_ = port;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetTargetUri(std::string_view uri) {
  target_uri_ = uri;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetTls(bool tls) {
  tls_ = tls;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetChannelCredentials(
    std::shared_ptr<grpc::ChannelCredentials> creds) {
  channel_credentials_ = std::move(creds);
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetSocketPath(std::string_view path) {
  socket_path_ = path;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetCertPath(std::string_view path) {
  cert_path_ = path;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetDeadlineMs(int deadline_ms) {
  deadline_ms_ = deadline_ms;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetStreamDeadlineMs(
    int stream_deadline_ms) {
  stream_deadline_ms_ = stream_deadline_ms;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetRetryBackoffMs(
    int retry_backoff_ms) {
  retry_backoff_ms_ = retry_backoff_ms;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetRetryBackoffMaxMs(
    int retry_backoff_max_ms) {
  retry_backoff_max_ms_ = retry_backoff_max_ms;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetRetryGracePeriod(
    int retry_grace_period) {
  retry_grace_period_ = retry_grace_period;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetKeepAliveTimeMs(
    int keep_alive_time_ms) {
  keep_alive_time_ms_ = keep_alive_time_ms;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetFatalStatusCodes(
    const std::vector<int>& fatal_status_codes) {
  fatal_status_codes_ = fatal_status_codes;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetFatalStatusCodes(
    const std::string& fatal_status_codes_str) {
  fatal_status_codes_ = ParseFatalStatusCodes(fatal_status_codes_str);
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetSelector(
    std::string_view selector) {
  selector_ = std::string(selector);
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetProviderId(
    std::string_view provider_id) {
  provider_id_ = std::string(provider_id);
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetOfflineFlagSourcePath(
    std::string_view path) {
  offline_flag_source_path_ = path;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::SetOfflinePollIntervalMs(
    int interval_ms) {
  offline_poll_interval_ms_ = interval_ms;
  return *this;
}

}  // namespace flagd
