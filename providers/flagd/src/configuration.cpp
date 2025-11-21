#include "flagd/configuration.h"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace flagd {

namespace {
struct EnvVars {
  static constexpr const char* HOST = "FLAGD_HOST";
  static constexpr const char* PORT = "FLAGD_PORT";
  static constexpr const char* TARGET_URI = "FLAGD_TARGET_URI";
  static constexpr const char* TLS = "FLAGD_TLS";
  static constexpr const char* SOCKET_PATH = "FLAGD_SOCKET_PATH";
  static constexpr const char* SERVER_CERT_PATH = "FLAGD_SERVER_CERT_PATH";
  static constexpr const char* DEADLINE_MS = "FLAGD_DEADLINE_MS";
  static constexpr const char* SOURCE_SELECTOR = "FLAGD_SOURCE_SELECTOR";
  static constexpr const char* PROVIDER_ID = "FLAGD_PROVIDER_ID";
  static constexpr const char* OFFLINE_FLAG_SOURCE_PATH =
      "FLAGD_OFFLINE_FLAG_SOURCE_PATH";
  static constexpr const char* OFFLINE_POLL_MS = "FLAGD_OFFLINE_POLL_MS";
};

struct Defaults {
  static constexpr const char* HOST = "localhost";
  static constexpr int PORT_IN_PROCESS = 8015;
  static constexpr bool TLS = false;
  static constexpr int DEADLINE_MS = 500;
  static constexpr int OFFLINE_POLL_MS = 5000;
};
}  // namespace

// --- Helpers ---
static std::string getEnvStr(const char* name,
                             const std::string& defaultValue = "") {
  const char* val = std::getenv(name);
  return val ? std::string(val) : defaultValue;
}

static int getEnvInt(const char* name, int defaultValue) {
  const char* val = std::getenv(name);
  if (val) {
    try {
      return std::stoi(val);
    } catch (...) {
      return defaultValue;
    }
  }
  return defaultValue;
}

static bool getEnvBool(const char* name, bool defaultValue) {
  const char* val = std::getenv(name);
  if (val) {
    std::string s(val);
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s == "true" || s == "1";
  }
  return defaultValue;
}

FlagdConfig::FlagdConfig() {
  host = Defaults::HOST;
  port = Defaults::PORT_IN_PROCESS;
  tls = Defaults::TLS;
  deadlineMs = Defaults::DEADLINE_MS;
  offlinePollIntervalMs = Defaults::OFFLINE_POLL_MS;

  loadFromEnv();
}

void FlagdConfig::loadFromEnv() {
  host = getEnvStr(EnvVars::HOST, Defaults::HOST);
  port = getEnvInt(EnvVars::PORT, Defaults::PORT_IN_PROCESS);

  std::string uriEnv = getEnvStr(EnvVars::TARGET_URI);
  if (!uriEnv.empty()) targetUri = uriEnv;

  tls = getEnvBool(EnvVars::TLS, Defaults::TLS);

  std::string sockEnv = getEnvStr(EnvVars::SOCKET_PATH);
  if (!sockEnv.empty()) socketPath = sockEnv;

  std::string certEnv = getEnvStr(EnvVars::SERVER_CERT_PATH);
  if (!certEnv.empty()) certPath = certEnv;

  deadlineMs = getEnvInt(EnvVars::DEADLINE_MS, Defaults::DEADLINE_MS);

  std::string selEnv = getEnvStr(EnvVars::SOURCE_SELECTOR);
  if (!selEnv.empty()) selector = selEnv;

  std::string pidEnv = getEnvStr(EnvVars::PROVIDER_ID);
  if (!pidEnv.empty()) providerId = pidEnv;

  std::string offEnv = getEnvStr(EnvVars::OFFLINE_FLAG_SOURCE_PATH);
  if (!offEnv.empty()) offlineFlagSourcePath = offEnv;

  offlinePollIntervalMs =
      getEnvInt(EnvVars::OFFLINE_POLL_MS, Defaults::OFFLINE_POLL_MS);
}

std::string FlagdConfig::getEffectiveTargetUri() const {
  if (targetUri.has_value() && !targetUri->empty()) return *targetUri;
  if (socketPath.has_value() && !socketPath->empty())
    return "unix://" + *socketPath;
  return host + ":" + std::to_string(port);
}

// --- Getters ---
const std::string& FlagdConfig::getHost() const { return host; }
int FlagdConfig::getPort() const { return port; }
std::optional<std::string> FlagdConfig::getTargetUri() const {
  return targetUri;
}
bool FlagdConfig::isTls() const { return tls; }
std::optional<std::string> FlagdConfig::getSocketPath() const {
  return socketPath;
}
std::optional<std::string> FlagdConfig::getCertPath() const { return certPath; }
int FlagdConfig::getDeadlineMs() const { return deadlineMs; }
std::optional<std::string> FlagdConfig::getSelector() const { return selector; }
std::optional<std::string> FlagdConfig::getProviderId() const {
  return providerId;
}
std::optional<std::string> FlagdConfig::getOfflineFlagSourcePath() const {
  return offlineFlagSourcePath;
}
int FlagdConfig::getOfflinePollIntervalMs() const {
  return offlinePollIntervalMs;
}

// --- Setters ---
FlagdConfig& FlagdConfig::setHost(std::string_view h) {
  host = h;
  return *this;
}
FlagdConfig& FlagdConfig::setPort(int p) {
  port = p;
  return *this;
}
FlagdConfig& FlagdConfig::setTargetUri(std::string_view uri) {
  targetUri = uri;
  return *this;
}
FlagdConfig& FlagdConfig::setTls(bool t) {
  tls = t;
  return *this;
}
FlagdConfig& FlagdConfig::setSocketPath(std::string_view path) {
  socketPath = path;
  return *this;
}
FlagdConfig& FlagdConfig::setCertPath(std::string_view path) {
  certPath = path;
  return *this;
}
FlagdConfig& FlagdConfig::setOfflineFlagSourcePath(std::string_view path) {
  offlineFlagSourcePath = path;
  return *this;
}
FlagdConfig& FlagdConfig::setSelector(std::string_view s) {
  selector = std::string(s);
  return *this;
}
FlagdConfig& FlagdConfig::setProviderId(std::string_view p) {
  providerId = std::string(p);
  return *this;
}

}  // namespace flagd