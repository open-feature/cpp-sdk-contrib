#include "flagd/evaluator.h"

#include <gtest/gtest.h>

#include <memory>
#include <nlohmann/json.hpp>

#include "flagd/sync.h"

class TestableSync : public flagd::FlagSync {
 public:
  using flagd::FlagSync::FlagSync;

  absl::Status Init(const openfeature::EvaluationContext& /*ctx*/) override {
    return absl::OkStatus();
  }
  absl::Status Shutdown() override { return absl::OkStatus(); }

  void TriggerUpdate(const nlohmann::json& new_json) {
    this->UpdateFlags(new_json);
  }
};

class EvaluatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    sync_ = std::make_shared<TestableSync>();
    evaluator_ = std::make_unique<flagd::JsonLogicEvaluator>(sync_);
  }

  std::shared_ptr<TestableSync> sync_;
  std::unique_ptr<flagd::JsonLogicEvaluator> evaluator_;
};

TEST_F(EvaluatorTest, ResolveBooleanSuccess) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "my-bool-flag": {
        "state": "ENABLED",
        "variants": {
          "on": true,
          "off": false
        },
        "defaultVariant": "on"
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("my-bool-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), true);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kStatic);
  EXPECT_EQ(result->GetVariant(), "on");
  EXPECT_FALSE(result->GetErrorCode().has_value());
}

TEST_F(EvaluatorTest, ResolveBooleanFlagNotFound) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {}
  })");
  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("missing-flag", true, ctx);

  EXPECT_EQ(result->GetValue(), true);  // Default value
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kFlagNotFound);
}

TEST_F(EvaluatorTest, ResolveBooleanTypeMismatch) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "my-string-flag": {
        "state": "ENABLED",
        "variants": {
          "on": "string-value",
          "off": "other-value"
        },
        "defaultVariant": "on"
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("my-string-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), false);  // Default value
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kTypeMismatch);
}

TEST_F(EvaluatorTest, ResolveBooleanMetadata) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "metadata": {
      "global-key": "global-value",
      "override-key": "global-override"
    },
    "flags": {
      "my-flag": {
        "state": "ENABLED",
        "variants": {
          "on": true,
          "off": false
        },
        "defaultVariant": "on",
        "metadata": {
          "flag-key": "flag-value",
          "override-key": "flag-override"
        }
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("my-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), true);
  const openfeature::FlagMetadata& metadata = result->GetFlagMetadata();

  EXPECT_EQ(std::get<std::string>(metadata.data.at("global-key")),
            "global-value");
  EXPECT_EQ(std::get<std::string>(metadata.data.at("flag-key")), "flag-value");
  EXPECT_EQ(std::get<std::string>(metadata.data.at("override-key")),
            "flag-override");
}

TEST_F(EvaluatorTest, ResolveBooleanVariantNotFound) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "my-broken-flag": {
        "state": "ENABLED",
        "variants": {
          "on": true,
          "off": false
        },
        "defaultVariant": "missing-variant"
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("my-broken-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), false);  // Default value
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kGeneral);
  EXPECT_EQ(result->GetErrorMessage(),
            "flag: my-broken-flag doesn't contain evaluated variant: "
            "missing-variant.");
}

TEST_F(EvaluatorTest, ResolveStringSuccess) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "my-string-flag": {
        "state": "ENABLED",
        "variants": {
          "v1": "value1",
          "v2": "value2"
        },
        "defaultVariant": "v1"
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::StringResolutionDetails> result =
      evaluator_->ResolveString("my-string-flag", "default", ctx);

  EXPECT_EQ(result->GetValue(), "value1");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kStatic);
  EXPECT_EQ(result->GetVariant(), "v1");
}

TEST_F(EvaluatorTest, ResolveIntegerSuccess) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "my-int-flag": {
        "state": "ENABLED",
        "variants": {
          "one": 1,
          "two": 2
        },
        "defaultVariant": "two"
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::IntResolutionDetails> result =
      evaluator_->ResolveInteger("my-int-flag", 0, ctx);

  EXPECT_EQ(result->GetValue(), 2);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kStatic);
  EXPECT_EQ(result->GetVariant(), "two");
}

TEST_F(EvaluatorTest, ResolveDoubleSuccess) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "my-double-flag": {
        "state": "ENABLED",
        "variants": {
          "d1": 1.1,
          "d2": 2.2
        },
        "defaultVariant": "d1"
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::DoubleResolutionDetails> result =
      evaluator_->ResolveDouble("my-double-flag", 0.0, ctx);

  EXPECT_DOUBLE_EQ(result->GetValue(), 1.1);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kStatic);
  EXPECT_EQ(result->GetVariant(), "d1");
}

TEST_F(EvaluatorTest, ResolveObjectSuccess) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "my-object-flag": {
        "state": "ENABLED",
        "variants": {
          "obj1": {
            "foo": "bar",
            "baz": 123
          },
          "obj2": {
            "key": true
          }
        },
        "defaultVariant": "obj1"
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::ObjectResolutionDetails> result =
      evaluator_->ResolveObject("my-object-flag", openfeature::Value(), ctx);

  openfeature::Value resolved_value = result->GetValue();

  EXPECT_TRUE(resolved_value.IsStructure());

  const std::map<std::string, openfeature::Value>* structure =
      resolved_value.AsStructure();
  ASSERT_NE(structure, nullptr);

  EXPECT_EQ(result->GetReason(), openfeature::Reason::kStatic);
  EXPECT_EQ(result->GetVariant(), "obj1");

  EXPECT_TRUE(structure->at("foo").IsString());
  EXPECT_EQ(structure->at("foo").AsString().value(), "bar");
  EXPECT_EQ(structure->at("baz").AsInt().value(), 123);
}

TEST_F(EvaluatorTest, ResolveStringTypeMismatch) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "my-int-flag": {
        "state": "ENABLED",
        "variants": {
          "one": 1
        },
        "defaultVariant": "one"
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::StringResolutionDetails> result =
      evaluator_->ResolveString("my-int-flag", "default", ctx);

  EXPECT_EQ(result->GetValue(), "default");
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kTypeMismatch);
}

TEST_F(EvaluatorTest, ResolveIntegerTypeMismatch) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "my-string-flag": {
        "state": "ENABLED",
        "variants": {
          "v1": "val1"
        },
        "defaultVariant": "v1"
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::IntResolutionDetails> result =
      evaluator_->ResolveInteger("my-string-flag", 0, ctx);

  EXPECT_EQ(result->GetValue(), 0);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kTypeMismatch);
}

TEST_F(EvaluatorTest, ResolveDoubleTypeMismatch) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "my-string-flag": {
        "state": "ENABLED",
        "variants": {
          "v1": "val1"
        },
        "defaultVariant": "v1"
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::DoubleResolutionDetails> result =
      evaluator_->ResolveDouble("my-string-flag", 0.0, ctx);

  EXPECT_EQ(result->GetValue(), 0.0);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
  EXPECT_EQ(result->GetErrorCode(), openfeature::ErrorCode::kTypeMismatch);
}

class EvaluatorDefaultVariantTest
    : public EvaluatorTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(EvaluatorDefaultVariantTest, ResolveBooleanReturnsDefault) {
  nlohmann::json flags_json =
      nlohmann::json::parse(R"({"flags":)" + GetParam() + "}");
  sync_->TriggerUpdate(flags_json);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("my-bool-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), false);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
  EXPECT_FALSE(result->GetErrorCode().has_value());
}

INSTANTIATE_TEST_SUITE_P(DefaultVariantHandling, EvaluatorDefaultVariantTest,
                         ::testing::Values(
                             // Missing default variant
                             R"({
          "my-bool-flag": {
            "state": "ENABLED",
            "variants": {
              "on": true,
              "off": false
            }
          }
        })",
                             // Null default variant
                             R"({
          "my-bool-flag": {
            "state": "ENABLED",
            "variants": {
              "on": true,
              "off": false
            },
            "defaultVariant": null
          }
        })"));

TEST_F(EvaluatorTest, ResolveBooleanDisabled) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "disabled-flag": {
        "state": "DISABLED",
        "variants": {
          "on": true,
          "off": false
        },
        "defaultVariant": "on"
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("disabled-flag", false, ctx);

  EXPECT_EQ(result->GetValue(), false);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDisabled);
}

TEST_F(EvaluatorTest, ResolveStringTargetingSuccess) {
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

  sync_->TriggerUpdate(flags);

  // Match targeting rule
  openfeature::EvaluationContext ctx_blue =
      openfeature::EvaluationContext::Builder()
          .WithAttribute("color", "blue")
          .build();
  std::unique_ptr<openfeature::StringResolutionDetails> result_blue =
      evaluator_->ResolveString("targeting-flag", "default", ctx_blue);
  EXPECT_EQ(result_blue->GetValue(), "blue-value");
  EXPECT_EQ(result_blue->GetReason(), openfeature::Reason::kTargetingMatch);
  EXPECT_EQ(result_blue->GetVariant(), "blue");

  // Fallback to default variant
  openfeature::EvaluationContext ctx_green =
      openfeature::EvaluationContext::Builder()
          .WithAttribute("color", "green")
          .build();
  std::unique_ptr<openfeature::StringResolutionDetails> result_green =
      evaluator_->ResolveString("targeting-flag", "default", ctx_green);
  EXPECT_EQ(result_green->GetValue(), "red-value");
  EXPECT_EQ(result_green->GetReason(), openfeature::Reason::kTargetingMatch);
  EXPECT_EQ(result_green->GetVariant(), "red");
}

TEST_F(EvaluatorTest, ResolveIntegerComplexTargeting) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "complex-flag": {
        "state": "ENABLED",
        "variants": {
          "low": 10,
          "high": 100
        },
        "defaultVariant": "low",
        "targeting": {
          "if": [
            {
              "and": [
                { "==": [{ "var": "env" }, "prod"] },
                { ">": [{ "var": "version" }, 1] }
              ]
            },
            "high",
            "low"
          ]
        }
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx = openfeature::EvaluationContext::Builder()
                                           .WithAttribute("env", "prod")
                                           .WithAttribute("version", 2)
                                           .build();
  std::unique_ptr<openfeature::IntResolutionDetails> result =
      evaluator_->ResolveInteger("complex-flag", 0, ctx);
  EXPECT_EQ(result->GetValue(), 100);
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kTargetingMatch);
}

TEST_F(EvaluatorTest, ResolveObjectNestedStructure) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "nested-flag": {
        "state": "ENABLED",
        "variants": {
          "v1": {
            "user": {
              "id": 123,
              "details": {
                "name": "John",
                "active": true
              }
            },
            "tags": ["a", "b", "c"]
          }
        },
        "defaultVariant": "v1"
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::ObjectResolutionDetails> result =
      evaluator_->ResolveObject("nested-flag", openfeature::Value(), ctx);

  openfeature::Value val = result->GetValue();
  ASSERT_TRUE(val.IsStructure());
  const std::map<std::string, openfeature::Value>* structure =
      val.AsStructure();

  ASSERT_TRUE(structure->at("user").IsStructure());
  const std::map<std::string, openfeature::Value>* user =
      structure->at("user").AsStructure();
  EXPECT_EQ(user->at("id").AsInt().value(), 123);

  const std::map<std::string, openfeature::Value>* details =
      user->at("details").AsStructure();
  EXPECT_EQ(details->at("name").AsString().value(), "John");
  EXPECT_TRUE(details->at("active").AsBool().value());

  ASSERT_TRUE(structure->at("tags").IsList());
  const std::vector<openfeature::Value>* tags = structure->at("tags").AsList();
  EXPECT_EQ(tags->size(), 3);
  EXPECT_EQ(tags->at(0).AsString().value(), "a");
}

TEST_F(EvaluatorTest, ResolveBooleanFlagdSpecialVars) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "special-var-flag": {
        "state": "ENABLED",
        "variants": {
          "on": true,
          "off": false
        },
        "defaultVariant": "off",
        "targeting": {
          "if": [
            { "==": [{ "var": "$flagd.flagKey" }, "special-var-flag"] },
            "on",
            "off"
          ]
        }
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("special-var-flag", false, ctx);
  EXPECT_TRUE(result->GetValue());
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kTargetingMatch);
}

TEST_F(EvaluatorTest, ResolveBooleanTypeMismatchInTargeting) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "bad-targeting-flag": {
        "state": "ENABLED",
        "variants": {
          "on": true,
          "off": false
        },
        "defaultVariant": "off",
        "targeting": { "var": "color" }
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx = openfeature::EvaluationContext::Builder()
                                           .WithAttribute("color", "blue")
                                           .build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("bad-targeting-flag", false, ctx);

  // Should fallback to default variant if targeting doesn't return a valid
  // variant name and return Error
  EXPECT_FALSE(result->GetValue());
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kError);
}

TEST_F(EvaluatorTest, ResolveBooleanDefaultVariantNull) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "null-default-flag": {
        "state": "ENABLED",
        "variants": {
          "on": true
        },
        "defaultVariant": null
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("null-default-flag", false, ctx);

  EXPECT_FALSE(result->GetValue());  // Should return default_value
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
}

TEST_F(EvaluatorTest, ResolveBooleanDefaultVariantMissing) {
  nlohmann::json flags = nlohmann::json::parse(R"({
    "flags": {
      "missing-default-flag": {
        "state": "ENABLED",
        "variants": {
          "on": true
        }
      }
    }
  })");

  sync_->TriggerUpdate(flags);

  openfeature::EvaluationContext ctx =
      openfeature::EvaluationContext::Builder().build();
  std::unique_ptr<openfeature::BoolResolutionDetails> result =
      evaluator_->ResolveBoolean("missing-default-flag", false, ctx);

  EXPECT_FALSE(result->GetValue());  // Should return default_value
  EXPECT_EQ(result->GetReason(), openfeature::Reason::kDefault);
}
