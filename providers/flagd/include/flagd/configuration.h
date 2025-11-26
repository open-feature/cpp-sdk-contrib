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
  const std::string& get_host() const;
  int get_port() const;
  std::optional<std::string> get_target_uri() const;
  bool get_tls() const;
  std::optional<std::string> get_socket_path() const;
  std::optional<std::string> get_cert_path() const;

  int get_deadline_ms() const;

  std::optional<std::string> get_selector() const;
  std::optional<std::string> get_provider_id() const;

  std::optional<std::string> get_offline_flag_source_path() const;
  int get_offline_poll_interval_ms() const;

  // --- Setters ---
  FlagdProviderConfig& set_host(std::string_view host);
  FlagdProviderConfig& set_port(int port);
  FlagdProviderConfig& set_target_uri(std::string_view uri);
  FlagdProviderConfig& set_tls(bool tls);
  FlagdProviderConfig& set_socket_path(std::string_view path);
  FlagdProviderConfig& set_cert_path(std::string_view path);
  FlagdProviderConfig& set_deadline_ms(int deadlineMs);
  FlagdProviderConfig& set_selector(std::string_view selector);
  FlagdProviderConfig& set_provider_id(std::string_view providerId);
  FlagdProviderConfig& set_offline_flag_source_path(std::string_view path);
  FlagdProviderConfig& set_offline_poll_interval_ms(int intervalMs);

  // --- Helper ---
  // Returns the effective Target URI used for gRPC connection.
  // Priority: Explicit TargetURI > SocketPath > Host:Port
  std::string get_effective_target_uri() const;

 private:
  std::string host_;
  int port_;
  std::optional<std::string> target_uri_;
  bool tls_;
  std::optional<std::string> socket_path_;
  std::optional<std::string> cert_path_;

  int deadline_ms_;

  std::optional<std::string> selector_;
  std::optional<std::string> provider_id_;

  std::optional<std::string> offline_flag_source_path_;
  int offline_poll_interval_ms_;
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_CONFIGURATION_H
