#include "flagd/configuration.h"

#include <gtest/gtest.h>

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
  EXPECT_EQ(config.GetHost(), "localhost");
  EXPECT_EQ(config.GetPort(), 8015);
  EXPECT_FALSE(config.GetTls());
  EXPECT_EQ(config.GetEffectiveTargetUri(), "localhost:8015");
}

TEST_F(ConfigurationTest, EnvironmentVariables) {
  setenv("FLAGD_HOST", "myhost", 1);
  setenv("FLAGD_PORT", "9000", 1);
  setenv("FLAGD_TLS", "true", 1);
  setenv("FLAGD_SOURCE_SELECTOR", "my-selector", 1);

  FlagdProviderConfig config;
  EXPECT_EQ(config.GetHost(), "myhost");
  EXPECT_EQ(config.GetPort(), 9000);
  EXPECT_TRUE(config.GetTls());
  EXPECT_TRUE(config.GetSelector().has_value());
  EXPECT_EQ(config.GetSelector().value(), "my-selector");
  EXPECT_EQ(config.GetEffectiveTargetUri(), "myhost:9000");
}

TEST_F(ConfigurationTest, EffectiveTargetUriPrecedence) {
  FlagdProviderConfig config;
  const int port = 1234;

  config.SetHost("host").SetPort(port);
  EXPECT_EQ(config.GetEffectiveTargetUri(), "host:1234");

  config.SetSocketPath("/tmp/flagd.sock");
  EXPECT_EQ(config.GetEffectiveTargetUri(), "unix:///tmp/flagd.sock");

  config.SetTargetUri("grpc://custom:5000");
  EXPECT_EQ(config.GetEffectiveTargetUri(), "grpc://custom:5000");
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
