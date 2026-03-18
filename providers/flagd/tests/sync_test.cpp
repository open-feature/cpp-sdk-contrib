#include "flagd/sync/sync.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

namespace flagd {

class TestableSync : public FlagSync {
 public:
  using FlagSync::FlagSync;

  absl::Status Init(const openfeature::EvaluationContext& ctx) override {
    return absl::OkStatus();
  }
  absl::Status Shutdown() override { return absl::OkStatus(); }

  void TriggerUpdate(const nlohmann::json& new_json) {
    this->UpdateFlags(new_json);
  }
};

class SyncTest : public ::testing::Test {
 protected:
  TestableSync sync_;
};

TEST_F(SyncTest, ValidatorAcceptsValidJson) {
  nlohmann::json valid_json = R"({
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
  })"_json;

  sync_.TriggerUpdate(valid_json);

  std::shared_ptr<const nlohmann::json> flags = sync_.GetFlags();
  EXPECT_TRUE(flags->contains("my-flag"));
}

TEST_F(SyncTest, ValidatorRejectsInvalidJson) {
  // Missing 'flags' field
  nlohmann::json invalid_json = R"({
    "something": "else"
  })"_json;

  sync_.TriggerUpdate(invalid_json);

  std::shared_ptr<const nlohmann::json> flags = sync_.GetFlags();
  EXPECT_TRUE(flags->empty());
}

TEST_F(SyncTest, ValidatorRejectsInvalidType) {
  // 'state' should be a string, not an integer
  nlohmann::json invalid_json = R"({
    "flags": {
      "my-flag": {
        "state": 123,
        "variants": {
          "on": true,
          "off": false
        },
        "defaultVariant": "on"
      }
    }
  })"_json;

  sync_.TriggerUpdate(invalid_json);

  std::shared_ptr<const nlohmann::json> flags = sync_.GetFlags();
  EXPECT_TRUE(flags->empty());
}

TEST_F(SyncTest, ValidatorRejectsMissingVariants) {
  // Missing 'variants' field
  nlohmann::json invalid_json = R"({
    "flags": {
      "my-flag": {
        "state": "ENABLED",
        "defaultVariant": "on"
      }
    }
  })"_json;

  sync_.TriggerUpdate(invalid_json);

  std::shared_ptr<const nlohmann::json> flags = sync_.GetFlags();
  EXPECT_TRUE(flags->empty());
}

TEST_F(SyncTest, ValidatorRejectsMalformedFlag) {
  // 'state' has invalid value
  nlohmann::json invalid_json = R"({
    "flags": {
      "my-flag": {
        "state": "INVALID_STATE",
        "variants": {
          "on": true
        },
        "defaultVariant": "on"
      }
    }
  })"_json;

  sync_.TriggerUpdate(invalid_json);

  std::shared_ptr<const nlohmann::json> flags = sync_.GetFlags();
  EXPECT_TRUE(flags->empty());
}

TEST_F(SyncTest, MetadataIsExtracted) {
  nlohmann::json json_with_metadata = R"({
    "flags": {},
    "metadata": {
      "foo": "bar"
    }
  })"_json;

  sync_.TriggerUpdate(json_with_metadata);

  std::shared_ptr<const nlohmann::json> metadata = sync_.GetMetadata();
  EXPECT_EQ((*metadata)["foo"], "bar");
}

}  // namespace flagd
