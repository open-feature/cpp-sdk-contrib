#include "flagd/sync.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

namespace flagd {

class TestableSync : public FlagSync {
 public:
  using FlagSync::FlagSync;

  absl::Status Init(const openfeature::EvaluationContext& /*ctx*/) override {
    return absl::OkStatus();
  }
  absl::Status Shutdown() override { return absl::OkStatus(); }

  void TriggerUpdate(const nlohmann::json& new_json) {
    this->UpdateFlags(new_json);
  }
};

TEST(SyncTest, ValidatorAcceptsValidJson) {
  TestableSync sync;
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

  sync.TriggerUpdate(valid_json);

  auto flags = sync.GetFlags();
  EXPECT_TRUE(flags->contains("my-flag"));
}

TEST(SyncTest, ValidatorRejectsInvalidJson) {
  TestableSync sync;
  // Missing 'flags' field
  nlohmann::json invalid_json = R"({
    "something": "else"
  })"_json;

  sync.TriggerUpdate(invalid_json);

  auto flags = sync.GetFlags();
  EXPECT_TRUE(flags->empty());
}

TEST(SyncTest, ValidatorRejectsInvalidType) {
  TestableSync sync;
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

  sync.TriggerUpdate(invalid_json);

  auto flags = sync.GetFlags();
  EXPECT_TRUE(flags->empty());
}

TEST(SyncTest, ValidatorRejectsMissingVariants) {
  TestableSync sync;
  // Missing 'variants' field
  nlohmann::json invalid_json = R"({
    "flags": {
      "my-flag": {
        "state": "ENABLED",
        "defaultVariant": "on"
      }
    }
  })"_json;

  sync.TriggerUpdate(invalid_json);

  auto flags = sync.GetFlags();
  EXPECT_TRUE(flags->empty());
}

TEST(SyncTest, ValidatorRejectsMalformedFlag) {
  TestableSync sync;
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

  sync.TriggerUpdate(invalid_json);

  auto flags = sync.GetFlags();
  EXPECT_TRUE(flags->empty());
}

TEST(SyncTest, MetadataIsExtracted) {
  TestableSync sync;
  nlohmann::json json_with_metadata = R"({
    "flags": {},
    "metadata": {
      "foo": "bar"
    }
  })"_json;

  sync.TriggerUpdate(json_with_metadata);

  auto metadata = sync.GetMetadata();
  EXPECT_EQ((*metadata)["foo"], "bar");
}

}  // namespace flagd
