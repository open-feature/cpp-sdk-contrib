#include "configuration.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "absl/strings/str_cat.h"

namespace flagd {

namespace {
struct EnvVars {
  static constexpr std::string_view kHost = "FLAGD_HOST";
  static constexpr std::string_view kPort = "FLAGD_PORT";
  static constexpr std::string_view kTargetUri = "FLAGD_TARGET_URI";
  static constexpr std::string_view kTls = "FLAGD_TLS";
  static constexpr std::string_view kSocketPath = "FLAGD_SOCKET_PATH";
  static constexpr std::string_view kServerCertPath = "FLAGD_SERVER_CERT_PATH";
  static constexpr std::string_view kDeadlineMs = "FLAGD_DEADLINE_MS";
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
  static constexpr int kOfflinePollMs = 5000;
};
}  // namespace

// --- Helpers ---
static std::string GetEnvStr(const std::string_view name,
                             const std::string_view default_value = "") {
  const char* val = std::getenv(std::string(name).c_str());
  return val ? std::string(val) : std::string(default_value);
}

static int GetEnvInt(const std::string_view name, int default_value) {
  const char* val = std::getenv(std::string(name).c_str());
  if (val) {
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
  if (!val) {
    return default_value;
  }
  std::string s(val);
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s == "true" || s == "1";
}

FlagdProviderConfig::FlagdProviderConfig()
    : host_(GetEnvStr(EnvVars::kHost, Defaults::kHost)),
      port_(GetEnvInt(EnvVars::kPort, Defaults::kPortInProcess)),
      tls_(GetEnvBool(EnvVars::kTls, Defaults::kTls)),
      deadline_ms_(GetEnvInt(EnvVars::kDeadlineMs, Defaults::kDeadlineMs)),
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
}

std::string FlagdProviderConfig::get_effective_target_uri() const {
  if (target_uri_.has_value() && !target_uri_->empty()) return *target_uri_;
  if (socket_path_.has_value() && !socket_path_->empty())
    return absl::StrCat("unix://", *socket_path_);
  return absl::StrCat(host_, ":", std::to_string(port_));
}

// --- Getters ---
const std::string& FlagdProviderConfig::get_host() const { return host_; }
int FlagdProviderConfig::get_port() const { return port_; }
std::optional<std::string> FlagdProviderConfig::get_target_uri() const {
  return target_uri_;
}
bool FlagdProviderConfig::get_tls() const { return tls_; }
std::optional<std::string> FlagdProviderConfig::get_socket_path() const {
  return socket_path_;
}
std::optional<std::string> FlagdProviderConfig::get_cert_path() const {
  return cert_path_;
}
int FlagdProviderConfig::get_deadline_ms() const { return deadline_ms_; }
std::optional<std::string> FlagdProviderConfig::get_selector() const {
  return selector_;
}
std::optional<std::string> FlagdProviderConfig::get_provider_id() const {
  return provider_id_;
}
std::optional<std::string> FlagdProviderConfig::get_offline_flag_source_path()
    const {
  return offline_flag_source_path_;
}
int FlagdProviderConfig::get_offline_poll_interval_ms() const {
  return offline_poll_interval_ms_;
}

// --- Setters ---
FlagdProviderConfig& FlagdProviderConfig::set_host(std::string_view host) {
  host_ = host;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::set_port(int port) {
  port_ = port;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::set_target_uri(std::string_view uri) {
  target_uri_ = uri;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::set_tls(bool tls) {
  tls_ = tls;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::set_socket_path(
    std::string_view path) {
  socket_path_ = path;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::set_cert_path(std::string_view path) {
  cert_path_ = path;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::set_deadline_ms(int deadline_ms) {
  deadline_ms_ = deadline_ms;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::set_selector(
    std::string_view selector) {
  selector_ = std::string(selector);
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::set_provider_id(
    std::string_view provider_id) {
  provider_id_ = std::string(provider_id);
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::set_offline_flag_source_path(
    std::string_view path) {
  offline_flag_source_path_ = path;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::set_offline_poll_interval_ms(
    int interval_ms) {
  offline_poll_interval_ms_ = interval_ms;
  return *this;
}

}  // namespace flagd
