#ifndef CPP_SDK_FLAGD_CONFIGURATION_H
#define CPP_SDK_FLAGD_CONFIGURATION_H

#include <optional>
#include <string>

namespace flagd {

enum class ResolverType { kRPC, kIN_PROCESS };

enum class CacheType { kLRU, kDISABLED };

class FlagdConfig {
 public:
  // --- Constructor ---
  // Initializes with defaults and overrides with Environment Variables.
  FlagdConfig();

  // --- Getters ---
  ResolverType getResolverType() const;
  std::string getHost() const;
  int getPort() const;
  std::optional<std::string> getTargetUri() const;
  bool isTls() const;
  std::optional<std::string> getSocketPath() const;
  std::optional<std::string> getCertPath() const;

  int getDeadlineMs() const;
  int getStreamDeadlineMs() const;
  int getRetryBackoffMs() const;
  int getRetryBackoffMaxMs() const;
  int getRetryGracePeriod() const;
  long getKeepAliveTime() const;

  CacheType getCacheType() const;
  int getMaxCacheSize() const;

  std::optional<std::string> getSelector() const;
  std::optional<std::string> getProviderId() const;

  std::optional<std::string> getOfflineFlagSourcePath() const;
  int getOfflinePollIntervalMs() const;

  // --- Setters ---
  FlagdConfig& setResolverType(ResolverType type);
  FlagdConfig& setHost(std::string_view host);
  FlagdConfig& setPort(int port);
  FlagdConfig& setTargetUri(std::string_view uri);
  FlagdConfig& setTls(bool tls);
  FlagdConfig& setSocketPath(std::string_view path);
  FlagdConfig& setCertPath(std::string_view path);
  FlagdConfig& setCacheType(CacheType type);
  FlagdConfig& setMaxCacheSize(int size);
  FlagdConfig& setOfflineFlagSourcePath(std::string_view path);

  // --- Helper ---
  // Returns the effective Target URI used for gRPC connection.
  // Priority: Explicit TargetURI > SocketPath > Host:Port
  std::string getEffectiveTargetUri() const;

 private:
  ResolverType resolverType;
  std::string host;
  int port;
  std::optional<std::string> targetUri;
  bool tls;
  std::optional<std::string> socketPath;
  std::optional<std::string> certPath;

  int deadlineMs;
  int streamDeadlineMs;
  int retryBackoffMs;
  int retryBackoffMaxMs;
  int retryGracePeriod;
  long keepAliveTime;

  CacheType cacheType;
  int maxCacheSize;

  std::optional<std::string> selector;
  std::optional<std::string> providerId;

  std::optional<std::string> offlineFlagSourcePath;
  int offlinePollIntervalMs;

  // Internal helper to load from env
  void loadFromEnv();
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_CONFIGURATION_H