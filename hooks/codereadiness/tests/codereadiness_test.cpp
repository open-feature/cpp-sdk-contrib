#include "codereadiness/codereadiness.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <any>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "codereadiness/version_comparator.h"
#include "openfeature/client.h"
#include "openfeature/error_code.h"
#include "openfeature/evaluation_context.h"
#include "openfeature/evaluation_options.h"
#include "openfeature/flag_evaluation_details.h"
#include "openfeature/flag_metadata.h"
#include "openfeature/memory_provider/flag.h"
#include "openfeature/memory_provider/in_memory_provider.h"
#include "openfeature/openfeature_api.h"
#include "openfeature/reason.h"

namespace codereadiness {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::openfeature::FlagMetadata;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::HasSubstr;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Return;
using ::testing::StartsWith;
using ::testing::StrEq;
using ::testing::StrictMock;

constexpr std::string_view kFlagKey = "my-flag";
constexpr std::string_view kDefaultKey = "minCodeVersion";

// A gMock implementation of VersionComparator, used to verify how the hook
// interacts with a custom comparator.
class MockVersionComparator : public VersionComparator {
 public:
  MOCK_METHOD(absl::Status, Initialize, (std::string_view current_version),
              (override));
  MOCK_METHOD(absl::StatusOr<bool>, Compare,
              (std::string_view required_version), (const, override));
};

FlagMetadata MetadataWithMinVersion(
    std::string min_version, std::string key = std::string(kDefaultKey)) {
  FlagMetadata metadata;
  metadata.data[std::move(key)] = std::move(min_version);
  return metadata;
}

std::shared_ptr<CodeReadinessHook> CreateHookOrDie(
    std::string current_version, CodeReadinessHookOptions options = {}) {
  absl::StatusOr<std::shared_ptr<CodeReadinessHook>> hook =
      CodeReadinessHook::Create(std::move(current_version), std::move(options));
  EXPECT_THAT(hook, IsOk());
  return hook.ok() ? *std::move(hook) : nullptr;
}

CodeReadinessHookOptions StrictOptions() {
  CodeReadinessHookOptions options;
  options.strict_validation = true;
  return options;
}

// Matches evaluation details for which the hook let the provider's resolved
// value through untouched.
MATCHER(PassedHook, "the hook did not fail the evaluation") {
  return ExplainMatchResult(Eq(std::nullopt), arg.GetErrorCode(),
                            result_listener) &&
         ExplainMatchResult(Eq(openfeature::Reason::kStatic), arg.GetReason(),
                            result_listener) &&
         ExplainMatchResult(Optional(StrEq("on")), arg.GetVariant(),
                            result_listener);
}

// Matches evaluation details for which the SDK caught an exception thrown by
// the hook: the default value is returned with reason ERROR, error code
// GENERAL and an error message matching `message_matcher`.
MATCHER_P(FailedHook, message_matcher,
          "the hook failed the evaluation with a matching message") {
  return ExplainMatchResult(Eq(openfeature::Reason::kError), arg.GetReason(),
                            result_listener) &&
         ExplainMatchResult(Optional(openfeature::ErrorCode::kGeneral),
                            arg.GetErrorCode(), result_listener) &&
         ExplainMatchResult(Optional(message_matcher), arg.GetErrorMessage(),
                            result_listener);
}

// Base fixture for end-to-end tests: the hook runs inside a real OpenFeature
// flag evaluation, backed by the SDK's InMemoryProvider. The global
// OpenFeatureAPI state is reset after every test.
class OpenFeatureE2ETest : public ::testing::Test {
 protected:
  void TearDown() override {
    openfeature::OpenFeatureAPI::GetInstance().Shutdown();
  }

  static std::shared_ptr<openfeature::Client> SetUpClient(
      std::shared_ptr<CodeReadinessHook> hook, FlagMetadata metadata) {
    SetUpProvider(std::move(metadata));
    std::shared_ptr<openfeature::Client> client =
        openfeature::OpenFeatureAPI::GetInstance().GetClient();
    client->AddHook(std::move(hook));
    return client;
  }

  static void SetUpProvider(FlagMetadata metadata) {
    std::unordered_map<std::string, std::any> flags;
    flags.emplace(std::string(kFlagKey),
                  openfeature::Flag<bool>({{"on", true}, {"off", false}},
                                          /*default_variant=*/"on",
                                          /*context_evaluator=*/nullptr,
                                          std::move(metadata)));
    openfeature::OpenFeatureAPI::GetInstance().SetProviderAndWait(
        std::make_shared<openfeature::InMemoryProvider>(std::move(flags)));
  }

  static openfeature::BoolFlagEvaluationDetails Evaluate(
      std::shared_ptr<CodeReadinessHook> hook, FlagMetadata metadata) {
    return SetUpClient(std::move(hook), std::move(metadata))
        ->GetBooleanDetails(kFlagKey, /*default_value=*/false);
  }
};

TEST(CodeReadinessHookCreateTest, SucceedsWithValidVersion) {
  absl::StatusOr<std::shared_ptr<CodeReadinessHook>> hook =
      CodeReadinessHook::Create("1.2.3");

  ASSERT_THAT(hook, IsOk());
  EXPECT_THAT(*hook, NotNull());
}

TEST(CodeReadinessHookCreateTest, FailsWithEmptyCurrentVersion) {
  EXPECT_THAT(CodeReadinessHook::Create(""),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("current_version cannot be empty")));
}

TEST(CodeReadinessHookCreateTest, FailsWithEmptyCurrentVersionInStrictMode) {
  EXPECT_THAT(CodeReadinessHook::Create("", StrictOptions()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("current_version cannot be empty")));
}

TEST(CodeReadinessHookCreateTest, FailsWithEmptyMetadataKey) {
  CodeReadinessHookOptions options;
  options.min_version_metadata_key = "";

  EXPECT_THAT(CodeReadinessHook::Create("1.2.3", std::move(options)),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("min_version_metadata_key cannot be empty")));
}

TEST(CodeReadinessHookCreateTest, InvalidCurrentVersionFailsInLenientMode) {
  EXPECT_THAT(
      CodeReadinessHook::Create("not-a-version"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("comparator initialization failed"),
                     HasSubstr("invalid current semver: \"not-a-version\""))));
}

TEST(CodeReadinessHookCreateTest, InvalidCurrentVersionFailsInStrictMode) {
  EXPECT_THAT(
      CodeReadinessHook::Create("not-a-version", StrictOptions()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("comparator initialization failed"),
                     HasSubstr("invalid current semver: \"not-a-version\""))));
}

TEST(CodeReadinessHookCreateTest,
     InitializesCustomComparatorWithCurrentVersion) {
  auto comparator = std::make_unique<StrictMock<MockVersionComparator>>();
  EXPECT_CALL(*comparator, Initialize("build-42"))
      .WillOnce(Return(absl::OkStatus()));
  CodeReadinessHookOptions options;
  options.comparator = std::move(comparator);

  EXPECT_THAT(CodeReadinessHook::Create("build-42", std::move(options)),
              IsOk());
}

TEST(CodeReadinessHookCreateTest, CustomComparatorInitFailureInStrictMode) {
  auto comparator = std::make_unique<StrictMock<MockVersionComparator>>();
  EXPECT_CALL(*comparator, Initialize(_))
      .WillOnce(Return(absl::InvalidArgumentError("custom failure")));
  CodeReadinessHookOptions options = StrictOptions();
  options.comparator = std::move(comparator);

  EXPECT_THAT(CodeReadinessHook::Create("build-42", std::move(options)),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("custom failure")));
}

TEST(CodeReadinessHookCreateTest, CustomComparatorInitFailureInLenientMode) {
  auto comparator = std::make_unique<StrictMock<MockVersionComparator>>();
  EXPECT_CALL(*comparator, Initialize(_))
      .WillOnce(Return(absl::InvalidArgumentError("custom failure")));
  CodeReadinessHookOptions options;
  options.comparator = std::move(comparator);

  EXPECT_THAT(CodeReadinessHook::Create("build-42", std::move(options)),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("custom failure")));
}

// Runs every version-check test in both modes, because the
// outcome of a successful comparison must not depend on `strict_validation`.
class CodeReadinessHookVersionCheckTest
    : public OpenFeatureE2ETest,
      public ::testing::WithParamInterface<bool> {
 protected:
  CodeReadinessHookOptions Options() const {
    CodeReadinessHookOptions options;
    options.strict_validation = GetParam();
    return options;
  }
};

TEST_P(CodeReadinessHookVersionCheckTest, PassesWhenCurrentEqualsRequired) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHookOrDie("1.2.3", Options());
  ASSERT_THAT(hook, NotNull());

  EXPECT_THAT(Evaluate(hook, MetadataWithMinVersion("1.2.3")), PassedHook());
}

TEST_P(CodeReadinessHookVersionCheckTest, PassesWhenCurrentIsHigher) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHookOrDie("2.0.0", Options());
  ASSERT_THAT(hook, NotNull());

  EXPECT_THAT(Evaluate(hook, MetadataWithMinVersion("1.9.9")), PassedHook());
}

TEST_P(CodeReadinessHookVersionCheckTest, PassesWithCLNumbers) {
  std::shared_ptr<CodeReadinessHook> hook =
      CreateHookOrDie("527319014", Options());
  ASSERT_THAT(hook, NotNull());

  EXPECT_THAT(Evaluate(hook, MetadataWithMinVersion("527319013")),
              PassedHook());
}

TEST_P(CodeReadinessHookVersionCheckTest, ThrowsWhenCurrentIsLower) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHookOrDie("1.2.3", Options());
  ASSERT_THAT(hook, NotNull());

  EXPECT_THAT(
      Evaluate(hook, MetadataWithMinVersion("1.3.0")),
      FailedHook(StrEq("codereadiness: flag \"my-flag\": current version "
                       "\"1.2.3\" is lower than required minimum version "
                       "\"1.3.0\"")));
}

TEST_P(CodeReadinessHookVersionCheckTest, ThrowsWhenCurrentIsPreRelease) {
  std::shared_ptr<CodeReadinessHook> hook =
      CreateHookOrDie("1.0.0-rc.1", Options());
  ASSERT_THAT(hook, NotNull());

  EXPECT_THAT(Evaluate(hook, MetadataWithMinVersion("1.0.0")),
              FailedHook(StartsWith("codereadiness: ")));
}

TEST_P(CodeReadinessHookVersionCheckTest, ThrowsWithLowerCLNumber) {
  std::shared_ptr<CodeReadinessHook> hook =
      CreateHookOrDie("527319012", Options());
  ASSERT_THAT(hook, NotNull());

  EXPECT_THAT(Evaluate(hook, MetadataWithMinVersion("527319013")),
              FailedHook(StartsWith("codereadiness: ")));
}

TEST_P(CodeReadinessHookVersionCheckTest, IgnoresUnrelatedMetadata) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHookOrDie("1.2.3", Options());
  ASSERT_THAT(hook, NotNull());
  FlagMetadata metadata = MetadataWithMinVersion("1.0.0");
  metadata.data["owner"] = std::string("team-a");
  constexpr int64_t kPriority = 5;
  metadata.data["priority"] = kPriority;

  EXPECT_THAT(Evaluate(hook, metadata), PassedHook());
}

INSTANTIATE_TEST_SUITE_P(StrictAndLenient, CodeReadinessHookVersionCheckTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "Strict" : "Lenient";
                         });

struct InvalidDataCase {
  std::string test_name;
  std::string current_version;
  FlagMetadata metadata;
  // Expected substring of the exception message in strict mode.
  std::string strict_message;
};

std::vector<InvalidDataCase> InvalidDataCases() {
  constexpr int64_t kIntegerMinVersion = 527319013;
  constexpr double kDoubleMinVersion = 1.5;

  FlagMetadata bool_value;
  bool_value.data[std::string(kDefaultKey)] = true;
  FlagMetadata int_value;
  int_value.data[std::string(kDefaultKey)] = kIntegerMinVersion;
  FlagMetadata double_value;
  double_value.data[std::string(kDefaultKey)] = kDoubleMinVersion;
  FlagMetadata other_key;
  other_key.data["otherKey"] = std::string("1.0.0");

  return {
      {"EmptyMetadata", "1.2.3", FlagMetadata{},
       R"(codereadiness: flag "my-flag": flag metadata is empty)"},
      {"MissingKey", "1.2.3", other_key,
       R"(codereadiness: flag "my-flag": metadata key "minCodeVersion" is missing)"},
      {"BoolValue", "1.2.3", bool_value,
       R"(codereadiness: flag "my-flag": metadata key "minCodeVersion" is not a string)"},
      {"IntegerValue", "1.2.3", int_value,
       R"(codereadiness: flag "my-flag": metadata key "minCodeVersion" is not a string)"},
      {"DoubleValue", "1.2.3", double_value,
       R"(codereadiness: flag "my-flag": metadata key "minCodeVersion" is not a string)"},
      {"EmptyValue", "1.2.3", MetadataWithMinVersion(""),
       R"(codereadiness: flag "my-flag": metadata key "minCodeVersion" value is empty)"},
      {"InvalidRequiredVersion", "1.2.3", MetadataWithMinVersion("1.x"),
       R"(codereadiness: flag "my-flag": invalid version values current: "1.2.3" - minimum: "1.x")"},
  };
}

std::string InvalidDataCaseName(
    const ::testing::TestParamInfo<InvalidDataCase>& info) {
  return info.param.test_name;
}

class CodeReadinessHookLenientTest
    : public OpenFeatureE2ETest,
      public ::testing::WithParamInterface<InvalidDataCase> {};

TEST_P(CodeReadinessHookLenientTest, SkipsValidation) {
  const InvalidDataCase& test_case = GetParam();
  std::shared_ptr<CodeReadinessHook> hook =
      CreateHookOrDie(test_case.current_version);
  ASSERT_THAT(hook, NotNull());

  EXPECT_THAT(Evaluate(hook, test_case.metadata), PassedHook());
}

INSTANTIATE_TEST_SUITE_P(InvalidData, CodeReadinessHookLenientTest,
                         ::testing::ValuesIn(InvalidDataCases()),
                         InvalidDataCaseName);

class CodeReadinessHookStrictTest
    : public OpenFeatureE2ETest,
      public ::testing::WithParamInterface<InvalidDataCase> {};

TEST_P(CodeReadinessHookStrictTest, ThrowsWithDescriptiveMessage) {
  const InvalidDataCase& test_case = GetParam();
  std::shared_ptr<CodeReadinessHook> hook =
      CreateHookOrDie(test_case.current_version, StrictOptions());
  ASSERT_THAT(hook, NotNull());

  EXPECT_THAT(Evaluate(hook, test_case.metadata),
              FailedHook(HasSubstr(test_case.strict_message)));
}

INSTANTIATE_TEST_SUITE_P(InvalidData, CodeReadinessHookStrictTest,
                         ::testing::ValuesIn(InvalidDataCases()),
                         InvalidDataCaseName);

using CodeReadinessHookMetadataKeyTest = OpenFeatureE2ETest;

TEST_F(CodeReadinessHookMetadataKeyTest, ReadsVersionFromCustomKey) {
  CodeReadinessHookOptions options;
  options.min_version_metadata_key = "requiredAppVersion";
  std::shared_ptr<CodeReadinessHook> hook =
      CreateHookOrDie("1.0.0", std::move(options));
  ASSERT_THAT(hook, NotNull());

  EXPECT_THAT(
      Evaluate(hook, MetadataWithMinVersion("2.0.0", "requiredAppVersion")),
      FailedHook(StartsWith("codereadiness: ")));
}

TEST_F(CodeReadinessHookMetadataKeyTest, IgnoresDefaultKeyWhenCustomKeyIsSet) {
  CodeReadinessHookOptions options = StrictOptions();
  options.min_version_metadata_key = "requiredAppVersion";
  std::shared_ptr<CodeReadinessHook> hook =
      CreateHookOrDie("1.0.0", std::move(options));
  ASSERT_THAT(hook, NotNull());

  // "minCodeVersion" would fail the check, but the hook must not read it.
  EXPECT_THAT(
      Evaluate(hook, MetadataWithMinVersion("2.0.0", std::string(kDefaultKey))),
      FailedHook(HasSubstr("metadata key \"requiredAppVersion\" is missing")));
}

class CodeReadinessHookCustomComparatorTest : public OpenFeatureE2ETest {
 protected:
  // Creates a hook owning a StrictMock comparator. `comparator_` stays valid
  // for the lifetime of `hook`.
  std::shared_ptr<CodeReadinessHook> CreateHook(bool strict_validation) {
    auto comparator = std::make_unique<StrictMock<MockVersionComparator>>();
    comparator_ = comparator.get();
    EXPECT_CALL(*comparator_, Initialize("build-42"))
        .WillOnce(Return(absl::OkStatus()));
    CodeReadinessHookOptions options;
    options.comparator = std::move(comparator);
    options.strict_validation = strict_validation;
    return CreateHookOrDie("build-42", std::move(options));
  }

  StrictMock<MockVersionComparator>* comparator_ = nullptr;
};

TEST_F(CodeReadinessHookCustomComparatorTest, PassesMetadataValueToCompare) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHook(/*strict=*/false);
  ASSERT_THAT(hook, NotNull());
  EXPECT_CALL(*comparator_, Compare("build-40")).WillOnce(Return(true));

  EXPECT_THAT(Evaluate(hook, MetadataWithMinVersion("build-40")), PassedHook());
}

TEST_F(CodeReadinessHookCustomComparatorTest, ThrowsWhenCompareReturnsFalse) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHook(/*strict=*/false);
  ASSERT_THAT(hook, NotNull());
  EXPECT_CALL(*comparator_, Compare("build-50")).WillOnce(Return(false));

  EXPECT_THAT(
      Evaluate(hook, MetadataWithMinVersion("build-50")),
      FailedHook(StrEq("codereadiness: flag \"my-flag\": current version "
                       "\"build-42\" is lower than required minimum version "
                       "\"build-50\"")));
}

TEST_F(CodeReadinessHookCustomComparatorTest, SkipsOnCompareErrorWhenLenient) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHook(/*strict=*/false);
  ASSERT_THAT(hook, NotNull());
  EXPECT_CALL(*comparator_, Compare(_))
      .WillOnce(Return(absl::InvalidArgumentError("bad build")));

  EXPECT_THAT(Evaluate(hook, MetadataWithMinVersion("build-x")), PassedHook());
}

TEST_F(CodeReadinessHookCustomComparatorTest, ThrowsOnCompareErrorWhenStrict) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHook(/*strict=*/true);
  ASSERT_THAT(hook, NotNull());
  EXPECT_CALL(*comparator_, Compare(_))
      .WillOnce(Return(absl::InvalidArgumentError("bad build")));

  EXPECT_THAT(Evaluate(hook, MetadataWithMinVersion("build-x")),
              FailedHook(AllOf(StartsWith("codereadiness: flag \"my-flag\": "),
                               HasSubstr("bad build"))));
}

TEST_F(CodeReadinessHookCustomComparatorTest,
       DoesNotCallCompareWhenMetadataIsMissing) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHook(/*strict=*/false);
  ASSERT_THAT(hook, NotNull());
  EXPECT_THAT(Evaluate(hook, FlagMetadata{}), PassedHook());
  EXPECT_THAT(Evaluate(hook, MetadataWithMinVersion("")), PassedHook());
}

using CodeReadinessHookIntegrationTest = OpenFeatureE2ETest;

TEST_F(CodeReadinessHookIntegrationTest, ReturnsDefaultValueWhenHookFails) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHookOrDie("1.0.0");
  ASSERT_THAT(hook, NotNull());
  std::shared_ptr<openfeature::Client> client =
      SetUpClient(hook, MetadataWithMinVersion("2.0.0"));

  EXPECT_FALSE(client->GetBooleanValue(kFlagKey, /*default_value=*/false));
}

TEST_F(CodeReadinessHookIntegrationTest, WorksAsGlobalHook) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHookOrDie("1.0.0");
  ASSERT_THAT(hook, NotNull());
  SetUpProvider(MetadataWithMinVersion("2.0.0"));
  openfeature::OpenFeatureAPI::GetInstance().AddHook(hook);

  EXPECT_THAT(
      openfeature::OpenFeatureAPI::GetInstance().GetClient()->GetBooleanDetails(
          kFlagKey, /*default_value=*/false),
      FailedHook(StartsWith("codereadiness: flag \"my-flag\": ")));
}

TEST_F(CodeReadinessHookIntegrationTest, WorksAsInvocationHook) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHookOrDie("1.0.0");
  ASSERT_THAT(hook, NotNull());
  SetUpProvider(MetadataWithMinVersion("2.0.0"));
  std::shared_ptr<openfeature::Client> client =
      openfeature::OpenFeatureAPI::GetInstance().GetClient();

  EXPECT_THAT(client->GetBooleanDetails(kFlagKey, /*default_value=*/false,
                                        openfeature::EvaluationOptions{{hook}}),
              FailedHook(StartsWith("codereadiness: flag \"my-flag\": ")));

  EXPECT_THAT(client->GetBooleanDetails(kFlagKey, /*default_value=*/false),
              PassedHook());
}

TEST_F(CodeReadinessHookIntegrationTest, WorksForNonBooleanFlags) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHookOrDie("1.0.0");
  ASSERT_THAT(hook, NotNull());
  std::unordered_map<std::string, std::any> flags;
  flags.emplace("string-flag", openfeature::Flag<std::string>(
                                   {{"v1", "value"}}, /*default_variant=*/"v1",
                                   /*context_evaluator=*/nullptr,
                                   MetadataWithMinVersion("2.0.0")));
  openfeature::OpenFeatureAPI::GetInstance().SetProviderAndWait(
      std::make_shared<openfeature::InMemoryProvider>(std::move(flags)));
  std::shared_ptr<openfeature::Client> client =
      openfeature::OpenFeatureAPI::GetInstance().GetClient();
  client->AddHook(hook);

  openfeature::StringFlagEvaluationDetails details =
      client->GetStringDetails("string-flag", "default");

  EXPECT_EQ(details.GetValue(), "default");
  EXPECT_THAT(details,
              FailedHook(StartsWith("codereadiness: flag \"string-flag\": ")));
}

TEST_F(CodeReadinessHookIntegrationTest, CanBeReusedAcrossEvaluations) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHookOrDie("1.5.0");
  ASSERT_THAT(hook, NotNull());

  EXPECT_THAT(Evaluate(hook, MetadataWithMinVersion("1.0.0")), PassedHook());
  EXPECT_THAT(Evaluate(hook, MetadataWithMinVersion("2.0.0")),
              FailedHook(StartsWith("codereadiness: ")));
  EXPECT_THAT(Evaluate(hook, MetadataWithMinVersion("1.5.0")), PassedHook());
}

}  // namespace codereadiness
