#ifndef CPP_SDK_FLAGD_CONFIGURATION_H
#define CPP_SDK_FLAGD_CONFIGURATION_H

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "grpcpp/security/credentials.h"

namespace flagd {

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
  int GetStreamDeadlineMs() const;
  int GetRetryBackoffMs() const;
  int GetRetryBackoffMaxMs() const;
  int GetRetryGracePeriod() const;
  int GetKeepAliveTimeMs() const;
  const std::vector<int>& GetFatalStatusCodes() const;

  std::string GetServiceConfigJson() const;

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
  FlagdProviderConfig& SetStreamDeadlineMs(int stream_deadline_ms);
  FlagdProviderConfig& SetRetryBackoffMs(int retry_backoff_ms);
  FlagdProviderConfig& SetRetryBackoffMaxMs(int retry_backoff_max_ms);
  FlagdProviderConfig& SetRetryGracePeriod(int retry_grace_period);
  FlagdProviderConfig& SetKeepAliveTimeMs(int keep_alive_time_ms);
  FlagdProviderConfig& SetFatalStatusCodes(
      const std::vector<int>& fatal_status_codes);
  FlagdProviderConfig& SetFatalStatusCodes(
      const std::string& fatal_status_codes_str);

  FlagdProviderConfig& SetSelector(std::string_view selector);
  FlagdProviderConfig& SetProviderId(std::string_view provider_id);
  FlagdProviderConfig& SetOfflineFlagSourcePath(std::string_view path);
  FlagdProviderConfig& SetOfflinePollIntervalMs(int interval_ms);

 private:
  std::string host_ = std::string(Defaults::kHost);
  int port_ = Defaults::kPortInProcess;
  std::optional<std::string> target_uri_;
  bool tls_ = Defaults::kTls;
  std::shared_ptr<grpc::ChannelCredentials> channel_credentials_;
  std::optional<std::string> socket_path_;
  std::optional<std::string> cert_path_;

  int deadline_ms_ = Defaults::kDeadlineMs;
  int stream_deadline_ms_ = Defaults::kStreamDeadlineMs;
  int retry_backoff_ms_ = Defaults::kRetryBackoffMs;
  int retry_backoff_max_ms_ = Defaults::kRetryBackoffMaxMs;
  int retry_grace_period_ = Defaults::kRetryGracePeriod;
  int keep_alive_time_ms_ = Defaults::kKeepAliveTimeMs;
  std::vector<int> fatal_status_codes_;

  std::optional<std::string> selector_;
  std::optional<std::string> provider_id_;

  std::optional<std::string> offline_flag_source_path_;
  int offline_poll_interval_ms_ = Defaults::kOfflinePollMs;
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_CONFIGURATION_H
