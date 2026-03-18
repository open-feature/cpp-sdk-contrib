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

TEST_F(ConfigurationTest, GetEffectiveCredentialsInsecure) {
  FlagdProviderConfig config;
  config.SetTls(false);
  auto creds = config.GetEffectiveCredentials();
  ASSERT_TRUE(creds.ok());
  EXPECT_NE(*creds, nullptr);
}

TEST_F(ConfigurationTest, GetEffectiveCredentialsTlsNoCert) {
  FlagdProviderConfig config;
  config.SetTls(true);
  auto creds = config.GetEffectiveCredentials();
  ASSERT_TRUE(creds.ok());
  EXPECT_NE(*creds, nullptr);
}

TEST_F(ConfigurationTest, GetEffectiveCredentialsExplicit) {
  FlagdProviderConfig config;
  auto my_creds = grpc::InsecureChannelCredentials();
  config.SetChannelCredentials(my_creds);
  auto creds = config.GetEffectiveCredentials();
  ASSERT_TRUE(creds.ok());
  EXPECT_EQ(*creds, my_creds);
}

}  // namespace flagd
