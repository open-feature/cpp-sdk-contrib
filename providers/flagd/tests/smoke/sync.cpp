#include "flagd/sync.h"

#include <memory>
#include <nlohmann/json.hpp>

#include "absl/status/status.h"
#include "gtest/gtest.h"

class TestableSync : public flagd::FlagSync {
 public:
  using flagd::FlagSync::FlagSync;

  absl::Status Init(const openfeature::EvaluationContext& ctx) override {
    return absl::OkStatus();
  }
  absl::Status Shutdown() override { return absl::OkStatus(); }

  void TriggerUpdate(const nlohmann::json& new_json) {
    this->UpdateFlags(new_json);
  }
};

class FlagSyncTest : public ::testing::Test {
 protected:
  TestableSync sync_;
};

TEST_F(FlagSyncTest, HelperMethodsUpdateAndRetrieveFlags) {
  // Checking if initialization works correctly and we will get empty json
  std::shared_ptr<const nlohmann::json> result_ptr = sync_.GetFlags();

  ASSERT_NE(result_ptr, nullptr)
      << "GetFlags() returned nullptr. The constructor should initialize it to "
         "an empty JSON object.";
  EXPECT_TRUE(result_ptr->is_object())
      << "Expected JSON type 'object' (e.g., {}), got: "
      << result_ptr->type_name();
  EXPECT_TRUE(result_ptr->empty())
      << "Expected empty JSON object, but found content.";

  // Checking if updating internal storage works correctly
  nlohmann::json expected_flags = R"({
    "flags": {
      "myFlag": {
        "state": "ENABLED",
        "variants": {
          "on": true,
          "off": false
        },
        "defaultVariant": "on"
      }
    }
  })"_json;

  sync_.TriggerUpdate(expected_flags);

  result_ptr = sync_.GetFlags();

  ASSERT_NE(result_ptr, nullptr)
      << "Flags pointer should not be null after update";
  EXPECT_EQ(*result_ptr, expected_flags["flags"])
      << "Retrieved JSON should match the updated JSON";
  EXPECT_EQ((*result_ptr)["myFlag"]["defaultVariant"], "on");
}

TEST_F(FlagSyncTest, InitAndShutdownReturnOk) {
  openfeature::EvaluationContext ctx;
  EXPECT_TRUE(sync_.Init(ctx).ok());
  EXPECT_TRUE(sync_.Shutdown().ok());
}

TEST_F(FlagSyncTest, ThreadSafety_ReadersAndWriters) {
  const int kReaderCount = 10;
  const int kIterations = 5000;
  std::atomic<bool> start_flag{false};

  auto writer_func = [&]() {
    while (!start_flag.load());

    for (int i = 0; i < kIterations; ++i) {
      nlohmann::json update = R"({
        "flags": {
          "myFlag": {
            "state": "ENABLED",
            "variants": {
              "iteration": 0
            },
            "defaultVariant": "iteration"
          }
        },
        "metadata": {}
      })"_json;
      update["flags"]["myFlag"]["variants"]["iteration"] = i;
      sync_.TriggerUpdate(update);
    }
  };

  auto reader_func = [&]() {
    while (!start_flag.load());

    for (int i = 0; i < kIterations; ++i) {
      auto flags = sync_.GetFlags();

      ASSERT_NE(flags, nullptr) << "Race condition detected: GetFlags returned "
                                   "nullptr during updates";

      if (flags->contains("myFlag") &&
          (*flags)["myFlag"].contains("variants") &&
          (*flags)["myFlag"]["variants"].contains("iteration")) {
        volatile int val =
            (*flags)["myFlag"]["variants"]["iteration"].get<int>();
        EXPECT_GE(val, 0) << "Non atomic store detected: Got incorrect value";
        EXPECT_LE(val, kIterations)
            << "Non atomic store detected: Got incorrect value";
      }
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < kReaderCount; ++i) {
    threads.emplace_back(reader_func);
  }
  threads.emplace_back(writer_func);

  start_flag.store(true);

  for (auto& t : threads) {
    if (t.joinable()) t.join();
  }

  auto final_flags = sync_.GetFlags();
  EXPECT_TRUE(final_flags->contains("myFlag"));
  EXPECT_TRUE((*final_flags)["myFlag"].contains("variants"));
  EXPECT_TRUE((*final_flags)["myFlag"]["variants"].contains("iteration"));
  EXPECT_EQ((*final_flags)["myFlag"]["variants"]["iteration"], kIterations - 1)
      << "";
}