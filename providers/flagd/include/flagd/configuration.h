#ifndef CPP_SDK_FLAGD_CONFIGURATION_H
#define CPP_SDK_FLAGD_CONFIGURATION_H

#include <optional>
#include <string>
#include <string_view>

namespace flagd {

class FlagdProviderConfig {
 public:
  // --- Constructor ---
  // Initializes with defaults and overrides with Environment Variables.
  FlagdProviderConfig();

  // --- Getters ---
  const std::string& getHost() const;
  int getPort() const;
  std::optional<std::string> getTargetUri() const;
  bool getTls() const;
  std::optional<std::string> getSocketPath() const;
  std::optional<std::string> getCertPath() const;

  int getDeadlineMs() const;

  std::optional<std::string> getSelector() const;
  std::optional<std::string> getProviderId() const;

  std::optional<std::string> getOfflineFlagSourcePath() const;
  int getOfflinePollIntervalMs() const;

  // --- Setters ---
  FlagdProviderConfig& setHost(std::string_view host);
  FlagdProviderConfig& setPort(int port);
  FlagdProviderConfig& setTargetUri(std::string_view uri);
  FlagdProviderConfig& setTls(bool tls);
  FlagdProviderConfig& setSocketPath(std::string_view path);
  FlagdProviderConfig& setCertPath(std::string_view path);
  FlagdProviderConfig& setDeadlineMs(int deadline_ms);
  FlagdProviderConfig& setSelector(std::string_view selector);
  FlagdProviderConfig& setProviderId(std::string_view providerId);
  FlagdProviderConfig& setOfflineFlagSourcePath(std::string_view path);
  FlagdProviderConfig& setOfflinePollIntervalMs(int intervalMs);

  // --- Helper ---
  // Returns the effective Target URI used for gRPC connection.
  // Priority: Explicit TargetURI > SocketPath > Host:Port
  std::string getEffectiveTargetUri() const;

 private:
  std::string host;
  int port;
  std::optional<std::string> targetUri;
  bool tls;
  std::optional<std::string> socketPath;
  std::optional<std::string> certPath;

  int deadlineMs;

  std::optional<std::string> selector;
  std::optional<std::string> providerId;

  std::optional<std::string> offlineFlagSourcePath;
  int offlinePollIntervalMs;
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_CONFIGURATION_H
