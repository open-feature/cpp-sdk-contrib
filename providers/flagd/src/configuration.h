#ifndef CPP_SDK_FLAGD_CONFIGURATION_H
#define CPP_SDK_FLAGD_CONFIGURATION_H

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "grpcpp/security/credentials.h"

namespace flagd {

class FlagdProviderConfig {
 public:
  // --- Constructor ---
  // Initializes with defaults and overrides with Environment Variables.
  FlagdProviderConfig();

  // --- Getters ---
  const std::string& GetHost() const;
  int GetPort() const;
  std::optional<std::string> GetTargetUri() const;
  bool GetTls() const;
  std::shared_ptr<grpc::ChannelCredentials> GetChannelCredentials() const;
  std::optional<std::string> GetSocketPath() const;
  std::optional<std::string> GetCertPath() const;

  int GetDeadlineMs() const;

  std::optional<std::string> GetSelector() const;
  std::optional<std::string> GetProviderId() const;

  std::optional<std::string> GetOfflineFlagSourcePath() const;
  int GetOfflinePollIntervalMs() const;

  // --- Helper ---
  // Returns the effective Target URI used for gRPC connection.
  // Priority: Explicit TargetURI > SocketPath > Host:Port
  std::string GetEffectiveTargetUri() const;

  // Returns the effective Channel Credentials used for gRPC connection.
  // Priority: Explicit ChannelCredentials > TLS/CertPath > Insecure
  absl::StatusOr<std::shared_ptr<grpc::ChannelCredentials>>
  GetEffectiveCredentials() const;

  // --- Setters ---
  FlagdProviderConfig& SetHost(std::string_view host);
  FlagdProviderConfig& SetPort(int port);
  FlagdProviderConfig& SetTargetUri(std::string_view uri);
  FlagdProviderConfig& SetTls(bool tls);
  FlagdProviderConfig& SetChannelCredentials(
      std::shared_ptr<grpc::ChannelCredentials> creds);
  FlagdProviderConfig& SetSocketPath(std::string_view path);
  FlagdProviderConfig& SetCertPath(std::string_view path);
  FlagdProviderConfig& SetDeadlineMs(int deadline_ms);
  FlagdProviderConfig& SetSelector(std::string_view selector);
  FlagdProviderConfig& SetProviderId(std::string_view provider_id);
  FlagdProviderConfig& SetOfflineFlagSourcePath(std::string_view path);
  FlagdProviderConfig& SetOfflinePollIntervalMs(int interval_ms);

 private:
  std::string host_;
  int port_;
  std::optional<std::string> target_uri_;
  bool tls_;
  std::shared_ptr<grpc::ChannelCredentials> channel_credentials_;
  std::optional<std::string> socket_path_;
  std::optional<std::string> cert_path_;

  int deadline_ms_;

  std::optional<std::string> selector_;
  std::optional<std::string> provider_id_;

  std::optional<std::string> offline_flag_source_path_;
  int offline_poll_interval_ms_;

  int Test;
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_CONFIGURATION_H
