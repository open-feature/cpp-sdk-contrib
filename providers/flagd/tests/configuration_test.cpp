#include "flagd/configuration.h"

#include <gtest/gtest.h>

#include "absl/strings/str_cat.h"

namespace flagd {

class ConfigurationTest : public ::testing::Test {
 protected:
  void SetUp() override { ClearFlagdEnvVars(); }

  void TearDown() override { ClearFlagdEnvVars(); }

 private:
  void ClearFlagdEnvVars() {
    unsetenv("FLAGD_HOST");
    unsetenv("FLAGD_PORT");
    unsetenv("FLAGD_TARGET_URI");
    unsetenv("FLAGD_TLS");
    unsetenv("FLAGD_SOCKET_PATH");
    unsetenv("FLAGD_SERVER_CERT_PATH");
    unsetenv("FLAGD_SOURCE_SELECTOR");
    unsetenv("FLAGD_PROVIDER_ID");
  }
};

TEST_F(ConfigurationTest, DefaultValues) {
  FlagdProviderConfig config;
  const std::string default_host = "localhost";
  const int default_port = 8015;
  EXPECT_EQ(config.GetHost(), default_host);
  EXPECT_EQ(config.GetPort(), default_port);
  EXPECT_FALSE(config.GetTls());
  EXPECT_EQ(config.GetEffectiveTargetUri(),
            absl::StrCat(default_host, ":", default_port));
}

TEST_F(ConfigurationTest, EnvironmentVariables) {
  const std::string host = "myhost";
  const int port = 9000;
  setenv("FLAGD_HOST", host.c_str(), 1);
  setenv("FLAGD_PORT", std::to_string(port).c_str(), 1);
  setenv("FLAGD_TLS", "true", 1);
  setenv("FLAGD_SOURCE_SELECTOR", "my-selector", 1);

  FlagdProviderConfig config;
  EXPECT_EQ(config.GetHost(), host);
  EXPECT_EQ(config.GetPort(), port);
  EXPECT_TRUE(config.GetTls());
  EXPECT_TRUE(config.GetSelector().has_value());
  EXPECT_EQ(config.GetSelector().value(), "my-selector");
  EXPECT_EQ(config.GetEffectiveTargetUri(), absl::StrCat(host, ":", port));
}

TEST_F(ConfigurationTest, EffectiveTargetUriPrecedence) {
  FlagdProviderConfig config;
  const std::string host = "host";
  const int port = 1234;

  config.SetHost(host).SetPort(port);
  EXPECT_EQ(config.GetEffectiveTargetUri(), absl::StrCat(host, ":", port));

  const std::string socket_path = "/tmp/flagd.sock";
  config.SetSocketPath(socket_path);
  EXPECT_EQ(config.GetEffectiveTargetUri(),
            absl::StrCat("unix://", socket_path));

  const std::string target_uri = "grpc://custom:5000";
  config.SetTargetUri(target_uri);
  EXPECT_EQ(config.GetEffectiveTargetUri(), target_uri);
}

TEST_F(ConfigurationTest, InvalidValues) {
  FlagdProviderConfig config;

  // Invalid Port
  int original_port = config.GetPort();
  config.SetPort(-1);
  EXPECT_EQ(config.GetPort(), original_port);
  config.SetPort(65536);
  EXPECT_EQ(config.GetPort(), original_port);

  // Invalid Timings
  int original_deadline = config.GetDeadlineMs();
  config.SetDeadlineMs(-1);
  EXPECT_EQ(config.GetDeadlineMs(), original_deadline);

  int original_stream_deadline = config.GetStreamDeadlineMs();
  config.SetStreamDeadlineMs(-1);
  EXPECT_EQ(config.GetStreamDeadlineMs(), original_stream_deadline);

  int original_retry_backoff = config.GetRetryBackoffMs();
  config.SetRetryBackoffMs(-1);
  EXPECT_EQ(config.GetRetryBackoffMs(), original_retry_backoff);

  int original_retry_backoff_max = config.GetRetryBackoffMaxMs();
  config.SetRetryBackoffMaxMs(-1);
  EXPECT_EQ(config.GetRetryBackoffMaxMs(), original_retry_backoff_max);

  int original_retry_grace = config.GetRetryGracePeriod();
  config.SetRetryGracePeriod(-1);
  EXPECT_EQ(config.GetRetryGracePeriod(), original_retry_grace);

  int original_keep_alive = config.GetKeepAliveTimeMs();
  config.SetKeepAliveTimeMs(-1);
  EXPECT_EQ(config.GetKeepAliveTimeMs(), original_keep_alive);

  int original_offline_poll = config.GetOfflinePollIntervalMs();
  config.SetOfflinePollIntervalMs(-1);
  EXPECT_EQ(config.GetOfflinePollIntervalMs(), original_offline_poll);

  // Invalid Fatal Status Codes
  config.SetFatalStatusCodes(std::vector<int>{1, 100, 5});  // 100 is invalid
  EXPECT_EQ(config.GetFatalStatusCodes().size(),
            0);  // Should be empty as it was ignored

  config.SetFatalStatusCodes("1,INVALID,5");  // INVALID is invalid
  EXPECT_EQ(config.GetFatalStatusCodes().size(), 2);
  EXPECT_EQ(config.GetFatalStatusCodes()[0], 1);
  EXPECT_EQ(config.GetFatalStatusCodes()[1], 5);
}

TEST_F(ConfigurationTest, GetEffectiveCredentialsInsecure) {
  FlagdProviderConfig config;
  config.SetTls(false);
  absl::StatusOr<std::shared_ptr<grpc::ChannelCredentials>> creds =
      config.GetEffectiveCredentials();
  ASSERT_TRUE(creds.ok());
  EXPECT_NE(*creds, nullptr);
}

TEST_F(ConfigurationTest, GetEffectiveCredentialsTlsNoCert) {
  FlagdProviderConfig config;
  config.SetTls(true);
  absl::StatusOr<std::shared_ptr<grpc::ChannelCredentials>> creds =
      config.GetEffectiveCredentials();
  ASSERT_TRUE(creds.ok());
  EXPECT_NE(*creds, nullptr);
}

TEST_F(ConfigurationTest, GetEffectiveCredentialsExplicit) {
  FlagdProviderConfig config;
  std::shared_ptr<grpc::ChannelCredentials> my_creds =
      grpc::InsecureChannelCredentials();
  config.SetChannelCredentials(my_creds);
  absl::StatusOr<std::shared_ptr<grpc::ChannelCredentials>> creds =
      config.GetEffectiveCredentials();
  ASSERT_TRUE(creds.ok());
  EXPECT_EQ(*creds, my_creds);
}

}  // namespace flagd
