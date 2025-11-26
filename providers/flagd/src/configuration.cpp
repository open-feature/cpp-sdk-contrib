#include "flagd/configuration.h"

#include <algorithm>
#include <cstdlib>
#include <string>

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
                             const std::string_view defaultValue = "") {
  const char* val = std::getenv(std::string(name).c_str());
  return val ? std::string(val) : std::string(defaultValue);
}

static int GetEnvInt(const std::string_view name, int defaultValue) {
  const char* val = std::getenv(std::string(name).c_str());
  if (val) {
    try {
      return std::stoi(val);
    } catch (...) {
      return defaultValue;
    }
  }
  return defaultValue;
}

static bool GetEnvBool(const std::string_view name, bool defaultValue) {
  const char* val = std::getenv(std::string(name).c_str());
  if (val) {
    std::string s(val);
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s == "true" || s == "1";
  }
  return defaultValue;
}

FlagdProviderConfig::FlagdProviderConfig() {
  host_ = GetEnvStr(EnvVars::kHost, Defaults::kHost);
  port_ = GetEnvInt(EnvVars::kPort, Defaults::kPortInProcess);

  std::string uri_env = GetEnvStr(EnvVars::kTargetUri);
  if (!uri_env.empty()) target_uri_ = uri_env;

  tls_ = GetEnvBool(EnvVars::kTls, Defaults::kTls);

  std::string sock_env = GetEnvStr(EnvVars::kSocketPath);
  if (!sock_env.empty()) socket_path_ = sock_env;

  std::string cert_env = GetEnvStr(EnvVars::kServerCertPath);
  if (!cert_env.empty()) cert_path_ = cert_env;

  deadline_ms_ = GetEnvInt(EnvVars::kDeadlineMs, Defaults::kDeadlineMs);

  std::string sel_env = GetEnvStr(EnvVars::kSourceSelector);
  if (!sel_env.empty()) selector_ = sel_env;

  std::string pid_env = GetEnvStr(EnvVars::kProviderId);
  if (!pid_env.empty()) provider_id_ = pid_env;

  std::string off_path_env = GetEnvStr(EnvVars::kOfflineFlagSourcePath);
  if (!off_path_env.empty()) offline_flag_source_path_ = off_path_env;

  offline_poll_interval_ms_ =
      GetEnvInt(EnvVars::kOfflinePollMs, Defaults::kOfflinePollMs);
}

std::string FlagdProviderConfig::get_effective_target_uri() const {
  if (target_uri_.has_value() && !target_uri_->empty()) return *target_uri_;
  if (socket_path_.has_value() && !socket_path_->empty())
    return "unix://" + *socket_path_;
  return host_ + ":" + std::to_string(port_);
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
    std::string_view providerId) {
  provider_id_ = std::string(providerId);
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::set_offline_flag_source_path(
    std::string_view path) {
  offline_flag_source_path_ = path;
  return *this;
}
FlagdProviderConfig& FlagdProviderConfig::set_offline_poll_interval_ms(
    int intervalMs) {
  offline_poll_interval_ms_ = intervalMs;
  return *this;
}

}  // namespace flagd
