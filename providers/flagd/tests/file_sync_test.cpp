#include "flagd/sync/file/file_sync.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unistd.h>

#include "flagd/configuration.h"
#include "openfeature/evaluation_context.h"

namespace flagd {
namespace {

const char* kValidFlags = R"({
  "flags": {
    "my-flag": {
      "state": "ENABLED",
      "variants": {
        "on": true,
        "off": false
      },
      "defaultVariant": "on"
    }
  }
})";

const char* kUpdatedFlags = R"({
  "flags": {
    "my-flag": {
      "state": "ENABLED",
      "variants": {
        "on": true,
        "off": false
      },
      "defaultVariant": "off"
    }
  }
})";

const char* kInvalidJson = R"({ not valid json )";

const char* kInvalidFlags = R"({
  "something": "else"
})";

class TempFile {
 public:
  explicit TempFile(const std::string& content) {
    std::string tmpl = "/tmp/file_sync_test_XXXXXX";
    int fd = mkstemp(tmpl.data());
    close(fd);
    path_ = tmpl;
    WriteContent(content);
  }

  ~TempFile() { std::remove(path_.c_str()); }

  const std::string& path() const { return path_; }

  void WriteContent(const std::string& content) {
    std::ofstream file(path_);
    file << content;
  }

 private:
  std::string path_;
};

class FileSyncTest : public ::testing::Test {
 protected:
  openfeature::EvaluationContext ctx_ =
      openfeature::EvaluationContext::Builder().build();
};

TEST_F(FileSyncTest, InitReadsValidFile) {
  TempFile file(kValidFlags);
  FlagdProviderConfig config;
  config.SetOfflineFlagSourcePath(file.path());

  FileSync sync(config);
  ASSERT_TRUE(sync.Init(ctx_).ok());

  auto flags = sync.GetFlags();
  ASSERT_TRUE(flags->contains("my-flag"));
  EXPECT_EQ((*flags)["my-flag"]["defaultVariant"], "on");

  ASSERT_TRUE(sync.Shutdown().ok());
}

TEST_F(FileSyncTest, InitFailsOnMissingFile) {
  FlagdProviderConfig config;
  config.SetOfflineFlagSourcePath("/nonexistent/path/to/flags.json");

  FileSync sync(config);
  absl::Status status = sync.Init(ctx_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
}

TEST_F(FileSyncTest, InitFailsOnInvalidJson) {
  TempFile file(kInvalidJson);
  FlagdProviderConfig config;
  config.SetOfflineFlagSourcePath(file.path());

  FileSync sync(config);
  absl::Status status = sync.Init(ctx_);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(FileSyncTest, InitWithInvalidFlagsSchemaKeepsEmptyFlags) {
  TempFile file(kInvalidFlags);
  FlagdProviderConfig config;
  config.SetOfflineFlagSourcePath(file.path());

  FileSync sync(config);
  // Init succeeds (file was read and parsed) but flags remain empty
  // because schema validation rejects the content
  ASSERT_TRUE(sync.Init(ctx_).ok());

  auto flags = sync.GetFlags();
  EXPECT_TRUE(flags->empty());

  ASSERT_TRUE(sync.Shutdown().ok());
}

TEST_F(FileSyncTest, PollDetectsFileChanges) {
  TempFile file(kValidFlags);
  FlagdProviderConfig config;
  config.SetOfflineFlagSourcePath(file.path());
  config.SetOfflinePollIntervalMs(100);

  FileSync sync(config);
  ASSERT_TRUE(sync.Init(ctx_).ok());

  auto flags = sync.GetFlags();
  EXPECT_EQ((*flags)["my-flag"]["defaultVariant"], "on");

  // Update the file
  file.WriteContent(kUpdatedFlags);

  // Wait for poll to pick up the change
  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  flags = sync.GetFlags();
  EXPECT_EQ((*flags)["my-flag"]["defaultVariant"], "off");

  ASSERT_TRUE(sync.Shutdown().ok());
}

TEST_F(FileSyncTest, ShutdownIsIdempotent) {
  TempFile file(kValidFlags);
  FlagdProviderConfig config;
  config.SetOfflineFlagSourcePath(file.path());

  FileSync sync(config);
  ASSERT_TRUE(sync.Init(ctx_).ok());
  ASSERT_TRUE(sync.Shutdown().ok());
  ASSERT_TRUE(sync.Shutdown().ok());
}

TEST_F(FileSyncTest, ShutdownWithoutInitIsOk) {
  TempFile file(kValidFlags);
  FlagdProviderConfig config;
  config.SetOfflineFlagSourcePath(file.path());

  FileSync sync(config);
  ASSERT_TRUE(sync.Shutdown().ok());
}

TEST_F(FileSyncTest, MetadataIsExtracted) {
  const char* flags_with_metadata = R"({
    "flags": {},
    "metadata": {
      "source": "file"
    }
  })";

  TempFile file(flags_with_metadata);
  FlagdProviderConfig config;
  config.SetOfflineFlagSourcePath(file.path());

  FileSync sync(config);
  ASSERT_TRUE(sync.Init(ctx_).ok());

  auto metadata = sync.GetMetadata();
  EXPECT_EQ((*metadata)["source"], "file");

  ASSERT_TRUE(sync.Shutdown().ok());
}

}  // namespace
}  // namespace flagd
