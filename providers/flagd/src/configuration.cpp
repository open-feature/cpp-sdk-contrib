#include "flagd/configuration.h"

#include <algorithm>
#include <cstdlib>

namespace flagd {

namespace {
struct EnvVars {
  static constexpr const char* RESOLVER_TYPE = "FLAGD_RESOLVER";
  static constexpr const char* HOST = "FLAGD_HOST";
  static constexpr const char* PORT = "FLAGD_PORT";
  static constexpr const char* TARGET_URI = "FLAGD_TARGET_URI";
  static constexpr const char* TLS = "FLAGD_TLS";
  static constexpr const char* SOCKET_PATH = "FLAGD_SOCKET_PATH";
  static constexpr const char* SERVER_CERT_PATH = "FLAGD_SERVER_CERT_PATH";
  static constexpr const char* DEADLINE_MS = "FLAGD_DEADLINE_MS";
  static constexpr const char* STREAM_DEADLINE_MS = "FLAGD_STREAM_DEADLINE_MS";
  static constexpr const char* RETRY_BACKOFF_MS = "FLAGD_RETRY_BACKOFF_MS";
  static constexpr const char* RETRY_BACKOFF_MAX_MS =
      "FLAGD_RETRY_BACKOFF_MAX_MS";
  static constexpr const char* RETRY_GRACE_PERIOD = "FLAGD_RETRY_GRACE_PERIOD";
  static constexpr const char* KEEP_ALIVE_TIME_MS = "FLAGD_KEEP_ALIVE_TIME_MS";
  static constexpr const char* CACHE_TYPE = "FLAGD_CACHE";
  static constexpr const char* MAX_CACHE_SIZE = "FLAGD_MAX_CACHE_SIZE";
  static constexpr const char* SOURCE_SELECTOR = "FLAGD_SOURCE_SELECTOR";
  static constexpr const char* PROVIDER_ID = "FLAGD_PROVIDER_ID";
  static constexpr const char* OFFLINE_FLAG_SOURCE_PATH =
      "FLAGD_OFFLINE_FLAG_SOURCE_PATH";
  static constexpr const char* OFFLINE_POLL_MS = "FLAGD_OFFLINE_POLL_MS";
};

struct Defaults {
  static constexpr const char* RESOLVER_TYPE = "rpc";
  static constexpr const char* HOST = "localhost";
  static constexpr int PORT_RPC = 8013;
  static constexpr int PORT_IN_PROCESS = 8015;
  static constexpr bool TLS = false;
  static constexpr int DEADLINE_MS = 500;
  static constexpr int STREAM_DEADLINE_MS = 600000;
  static constexpr int RETRY_BACKOFF_MS = 1000;
  static constexpr int RETRY_BACKOFF_MAX_MS = 120000;
  static constexpr int RETRY_GRACE_PERIOD = 5;
  static constexpr long KEEP_ALIVE_TIME = 0;
  static constexpr const char* CACHE_TYPE = "lru";
  static constexpr int MAX_CACHE_SIZE = 1000;
  static constexpr int OFFLINE_POLL_MS = 5000;
};
}  // namespace

// --- Helper Functions for Env Vars ---

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

static long getEnvLong(const char* name, long defaultValue) {
  const char* val = std::getenv(name);
  if (val) {
    try {
      return std::stol(val);
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

// --- Implementation ---

FlagdConfig::FlagdConfig() {
  // 1. Set hardcoded defaults
  resolverType = ResolverType::kRPC;
  host = Defaults::HOST;
  port = Defaults::PORT_RPC;
  tls = Defaults::TLS;
  deadlineMs = Defaults::DEADLINE_MS;
  streamDeadlineMs = Defaults::STREAM_DEADLINE_MS;
  retryBackoffMs = Defaults::RETRY_BACKOFF_MS;
  retryBackoffMaxMs = Defaults::RETRY_BACKOFF_MAX_MS;
  retryGracePeriod = Defaults::RETRY_GRACE_PERIOD;
  keepAliveTime = Defaults::KEEP_ALIVE_TIME;
  cacheType = CacheType::kLRU;
  maxCacheSize = Defaults::MAX_CACHE_SIZE;
  offlinePollIntervalMs = Defaults::OFFLINE_POLL_MS;

  // 2. Override with Environment Variables
  loadFromEnv();
}

void FlagdConfig::loadFromEnv() {
  // Resolver
  std::string resEnv =
      getEnvStr(EnvVars::RESOLVER_TYPE, Defaults::RESOLVER_TYPE);
  if (resEnv == "in-process")
    resolverType = ResolverType::kIN_PROCESS;
  else
    resolverType = ResolverType::kRPC;

  // Host
  host = getEnvStr(EnvVars::HOST, Defaults::HOST);

  // Port - Default changes based on resolver type
  int defaultPort = (resolverType == ResolverType::kIN_PROCESS)
                        ? Defaults::PORT_IN_PROCESS
                        : Defaults::PORT_RPC;

  port = getEnvInt(EnvVars::PORT, defaultPort);

  // Target URI
  std::string uriEnv = getEnvStr(EnvVars::TARGET_URI);
  if (!uriEnv.empty()) targetUri = uriEnv;

  // TLS
  tls = getEnvBool(EnvVars::TLS, Defaults::TLS);

  // Socket Path
  std::string sockEnv = getEnvStr(EnvVars::SOCKET_PATH);
  if (!sockEnv.empty()) socketPath = sockEnv;

  // Cert Path
  std::string certEnv = getEnvStr(EnvVars::SERVER_CERT_PATH);
  if (!certEnv.empty()) certPath = certEnv;

  // Timeouts & Retries
  deadlineMs = getEnvInt(EnvVars::DEADLINE_MS, Defaults::DEADLINE_MS);
  streamDeadlineMs =
      getEnvInt(EnvVars::STREAM_DEADLINE_MS, Defaults::STREAM_DEADLINE_MS);
  retryBackoffMs =
      getEnvInt(EnvVars::RETRY_BACKOFF_MS, Defaults::RETRY_BACKOFF_MS);
  retryBackoffMaxMs =
      getEnvInt(EnvVars::RETRY_BACKOFF_MAX_MS, Defaults::RETRY_BACKOFF_MAX_MS);
  retryGracePeriod =
      getEnvInt(EnvVars::RETRY_GRACE_PERIOD, Defaults::RETRY_GRACE_PERIOD);
  keepAliveTime =
      getEnvLong(EnvVars::KEEP_ALIVE_TIME_MS, Defaults::KEEP_ALIVE_TIME);

  // Cache
  std::string cacheEnv = getEnvStr(EnvVars::CACHE_TYPE, Defaults::CACHE_TYPE);
  if (cacheEnv == "disabled")
    cacheType = CacheType::kDISABLED;
  else
    cacheType = CacheType::kLRU;

  maxCacheSize = getEnvInt(EnvVars::MAX_CACHE_SIZE, Defaults::MAX_CACHE_SIZE);

  // Selector & Provider ID
  std::string selEnv = getEnvStr(EnvVars::SOURCE_SELECTOR);
  if (!selEnv.empty()) selector = selEnv;

  std::string pidEnv = getEnvStr(EnvVars::PROVIDER_ID);
  if (!pidEnv.empty()) providerId = pidEnv;

  // Offline / File
  std::string offEnv = getEnvStr(EnvVars::OFFLINE_FLAG_SOURCE_PATH);
  if (!offEnv.empty()) offlineFlagSourcePath = offEnv;

  offlinePollIntervalMs =
      getEnvInt(EnvVars::OFFLINE_POLL_MS, Defaults::OFFLINE_POLL_MS);
}

std::string FlagdConfig::getEffectiveTargetUri() const {
  // 1. Explicit Target URI
  if (targetUri.has_value() && !targetUri->empty()) {
    return *targetUri;
  }

  // 2. Socket Path (Unix sockets)
  if (socketPath.has_value() && !socketPath->empty()) {
    return "unix://" + *socketPath;
  }

  // 3. Host : Port
  return host + ":" + std::to_string(port);
}

// --- Getters ---
ResolverType FlagdConfig::getResolverType() const { return resolverType; }
std::string FlagdConfig::getHost() const { return host; }
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
int FlagdConfig::getStreamDeadlineMs() const { return streamDeadlineMs; }
int FlagdConfig::getRetryBackoffMs() const { return retryBackoffMs; }
int FlagdConfig::getRetryBackoffMaxMs() const { return retryBackoffMaxMs; }
int FlagdConfig::getRetryGracePeriod() const { return retryGracePeriod; }
long FlagdConfig::getKeepAliveTime() const { return keepAliveTime; }
CacheType FlagdConfig::getCacheType() const { return cacheType; }
int FlagdConfig::getMaxCacheSize() const { return maxCacheSize; }
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
FlagdConfig& FlagdConfig::setResolverType(ResolverType type) {
  resolverType = type;
  return *this;
}
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
FlagdConfig& FlagdConfig::setCacheType(CacheType type) {
  cacheType = type;
  return *this;
}
FlagdConfig& FlagdConfig::setMaxCacheSize(int size) {
  maxCacheSize = size;
  return *this;
}
FlagdConfig& FlagdConfig::setOfflineFlagSourcePath(std::string_view path) {
  offlineFlagSourcePath = path;
  return *this;
}

}  // namespace flagd