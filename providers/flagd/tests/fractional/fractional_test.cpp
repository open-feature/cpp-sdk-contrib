#include <gtest/gtest.h>
#include <openfeature/evaluation_context.h>
#include <openfeature/openfeature_api.h>
#include <openfeature/reason.h>
#include <openfeature/resolution_details.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>

#include "absl/strings/str_cat.h"
#include "flagd/provider.h"
#include "flagd/sync/sync.h"

namespace flagd {

class MockSync : public FlagSync {
 public:
  using FlagSync::FlagSync;
  absl::Status Init(const openfeature::EvaluationContext& /*ctx*/) override {
    nlohmann::json flags = R"({
      "flags": {
        "basic-flag": {
          "state": "ENABLED",
          "variants": {
            "true": true,
            "false": false
          },
          "defaultVariant": "false",
          "targeting": {
            "fractional": [
              {
                "var": "json"
              },
              [
                "true",
                100
              ],
              [
                "false",
                0
              ]
            ]
          }
        }
      }
    })"_json;
    this->UpdateFlags(flags);
    return absl::OkStatus();
  }
  absl::Status Shutdown() override { return absl::OkStatus(); }
};

openfeature::Value JsonToValue(const nlohmann::json& json_val) {
  if (json_val.is_boolean()) {
    return {json_val.get<bool>()};
  }
  if (json_val.is_number_integer()) {
    return {json_val.get<int64_t>()};
  }
  if (json_val.is_number_float()) {
    return {json_val.get<double>()};
  }
  if (json_val.is_string()) {
    return {json_val.get<std::string>()};
  }
  if (json_val.is_object()) {
    std::map<std::string, openfeature::Value> map;
    for (const auto& [key, value] : json_val.items()) {
      map.emplace(key, JsonToValue(value));
    }
    return {map};
  }
  if (json_val.is_array()) {
    std::vector<openfeature::Value> vec;
    vec.reserve(json_val.size());
    for (const auto& item : json_val) {
      vec.push_back(JsonToValue(item));
    }
    return {vec};
  }
  return {};
}

class FractionalTest : public ::testing::Test {
 protected:
  void SetUp() override {
    sync_ = std::make_shared<MockSync>();
    provider_ = std::make_shared<FlagdProvider>(sync_);
    openfeature::OpenFeatureAPI& api =
        openfeature::OpenFeatureAPI::GetInstance();
    api.SetProviderAndWait(provider_);
    client_ = api.GetClient();
  }

  void TearDown() override {
    openfeature::OpenFeatureAPI::GetInstance().Shutdown();
  }

  std::shared_ptr<MockSync> sync_;
  std::shared_ptr<FlagdProvider> provider_;
  std::shared_ptr<openfeature::Client> client_;
};

TEST_F(FractionalTest, RunAllTests) {
  std::string test_dir = "providers/flagd/tests/fractional/test1000";

  std::vector<int> test_ids;
  for (const auto& entry : std::filesystem::directory_iterator(test_dir)) {
    if (entry.path().extension() == ".json") {
      try {
        test_ids.push_back(std::stoi(entry.path().stem().string()));
      } catch (...) {
        // Skip files that are not numbers
      }
    }
  }
  std::sort(test_ids.begin(), test_ids.end());

  for (int tid : test_ids) {
    std::string test_id = std::to_string(tid);
    std::string file_path = absl::StrCat(test_dir, "/", test_id, ".json");

    std::ifstream file_stream(file_path);
    nlohmann::json test_input;
    file_stream >> test_input;

    openfeature::EvaluationContext context =
        openfeature::EvaluationContext::Builder()
            .WithTargetingKey(test_id)
            .WithAttribute("json", JsonToValue(test_input))
            .WithAttribute("id", test_id)
            .WithAttribute("path", test_dir)
            .build();

    bool result = client_->GetBooleanValue("basic-flag", false, context);

    EXPECT_TRUE(result) << absl::StrCat("Test ", test_id,
                                        " failed: expected true, got false "
                                        "(likely verification mismatch)");
  }
}

}  // namespace flagd
