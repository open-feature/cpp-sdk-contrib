#include <gtest/gtest.h>

#include <memory>
#include <nlohmann/json.hpp>

#include "flagd/provider/provider.h"
#include "flagd/sync/sync.h"
#include "openfeature/openfeature_api.h"

namespace {

class MockSync : public flagd::FlagSync {
 public:
  absl::Status Init(const openfeature::EvaluationContext& /*ctx*/) override {
    return absl::OkStatus();
  }
  absl::Status Shutdown() override { return absl::OkStatus(); }

  void SetFlags(const nlohmann::json& flags) { this->UpdateFlags(flags); }
};

class FlagdOpenFeatureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    sync_ = std::make_shared<MockSync>();
    provider_ = std::make_shared<flagd::FlagdProvider>(sync_);
    auto& api = openfeature::OpenFeatureAPI::GetInstance();
    api.SetProviderAndWait(provider_);
    client_ = api.GetClient();
  }

  void TearDown() override {
    auto& api = openfeature::OpenFeatureAPI::GetInstance();
    api.Shutdown();
  }

  std::shared_ptr<MockSync> sync_;
  std::shared_ptr<flagd::FlagdProvider> provider_;
  std::shared_ptr<openfeature::Client> client_;
};

TEST_F(FlagdOpenFeatureTest, BooleanEvaluation) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "bool-flag": {
        "state": "ENABLED",
        "variants": {
          "on": true,
          "off": false
        },
        "defaultVariant": "on"
      }
    }
  })");
  sync_->SetFlags(flags);

  EXPECT_TRUE(client_->GetBooleanValue("bool-flag", false));
  EXPECT_FALSE(client_->GetBooleanValue("non-existent", false));
}

TEST_F(FlagdOpenFeatureTest, StringEvaluation) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "string-flag": {
        "state": "ENABLED",
        "variants": {
          "v1": "value1",
          "v2": "value2"
        },
        "defaultVariant": "v2"
      }
    }
  })");
  sync_->SetFlags(flags);

  EXPECT_EQ(client_->GetStringValue("string-flag", "default"), "value2");
}

TEST_F(FlagdOpenFeatureTest, IntegerEvaluation) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "int-flag": {
        "state": "ENABLED",
        "variants": {
          "one": 1,
          "two": 2
        },
        "defaultVariant": "one"
      }
    }
  })");
  sync_->SetFlags(flags);

  EXPECT_EQ(client_->GetIntegerValue("int-flag", 0), 1);
}

TEST_F(FlagdOpenFeatureTest, DoubleEvaluation) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "double-flag": {
        "state": "ENABLED",
        "variants": {
          "d1": 1.1,
          "d2": 2.2
        },
        "defaultVariant": "d2"
      }
    }
  })");
  sync_->SetFlags(flags);

  EXPECT_DOUBLE_EQ(client_->GetDoubleValue("double-flag", 0.0), 2.2);
}

TEST_F(FlagdOpenFeatureTest, ObjectEvaluation) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "obj-flag": {
        "state": "ENABLED",
        "variants": {
          "v1": {
            "key": "value"
          }
        },
        "defaultVariant": "v1"
      }
    }
  })");
  sync_->SetFlags(flags);

  openfeature::Value val =
      client_->GetObjectValue("obj-flag", openfeature::Value());
  ASSERT_TRUE(val.IsStructure());
  const std::map<std::string, openfeature::Value>* structure =
      val.AsStructure();
  EXPECT_EQ(structure->at("key").AsString().value(), "value");
}

TEST_F(FlagdOpenFeatureTest, EvaluationContextTargeting) {
  // Example of targeting using EvaluationContext
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "targeting-flag": {
        "state": "ENABLED",
        "variants": {
          "red": "red-value",
          "blue": "blue-value"
        },
        "defaultVariant": "red",
        "targeting": {
          "if": [
            { "==": [{ "var": "color" }, "blue"] },
            "blue",
            "red"
          ]
        }
      }
    }
  })");
  sync_->SetFlags(flags);

  // Without context, should return defaultVariant "red"
  EXPECT_EQ(client_->GetStringValue("targeting-flag", "default"), "red-value");

  // With context color=blue, should return "blue"
  openfeature::EvaluationContext ctx = openfeature::EvaluationContext::Builder()
                                           .WithAttribute("color", "blue")
                                           .build();
  EXPECT_EQ(client_->GetStringValue("targeting-flag", "default", ctx),
            "blue-value");

  // With context color=green, should return "red"
  openfeature::EvaluationContext ctx2 =
      openfeature::EvaluationContext::Builder()
          .WithAttribute("color", "green")
          .build();
  EXPECT_EQ(client_->GetStringValue("targeting-flag", "default", ctx2),
            "red-value");
}

}  // namespace
