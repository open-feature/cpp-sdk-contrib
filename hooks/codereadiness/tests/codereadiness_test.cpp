#include "codereadiness/codereadiness.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "codereadiness/version_comparator.h"
#include "openfeature/evaluation_context.h"
#include "openfeature/flag_evaluation_details.h"
#include "openfeature/flag_metadata.h"
#include "openfeature/flag_type_value.h"
#include "openfeature/general_hook.h"
#include "openfeature/hook_context.h"
#include "openfeature/hook_data.h"
#include "openfeature/hook_hints.h"
#include "openfeature/metadata.h"
#include "openfeature/reason.h"

namespace codereadiness {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::openfeature::FlagMetadata;
using ::testing::_;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::ThrowsMessage;

constexpr char kFlagKey[] = "my-flag";
constexpr char kDefaultKey[] = "minCodeVersion";
constexpr char kMetadataClient[] = "client";
constexpr char kMetadataProvider[] = "provider";

// A gMock implementation of VersionComparator, used to verify how the hook
// interacts with a custom comparator.
class MockVersionComparator : public VersionComparator {
 public:
  MOCK_METHOD(absl::Status, Initialize, (std::string_view current_version),
              (override));
  MOCK_METHOD(absl::StatusOr<bool>, Compare,
              (std::string_view required_version), (const, override));
};

openfeature::BoolHookContext MakeHookContext(std::string flag_key = kFlagKey) {
  return openfeature::BoolHookContext(
      std::move(flag_key), openfeature::FlagValueType::kBoolean,
      /*default_value=*/false,
      openfeature::EvaluationContext::Builder().build(),
      openfeature::Metadata{kMetadataClient},
      openfeature::Metadata{kMetadataProvider},
      std::make_shared<openfeature::HookData>());
}

openfeature::BoolFlagEvaluationDetails MakeDetails(FlagMetadata metadata) {
  return openfeature::BoolFlagEvaluationDetails(
      kFlagKey, /*value=*/true, openfeature::Reason::kTargetingMatch,
      /*variant=*/"on", metadata);
}

FlagMetadata MetadataWithMinVersion(std::string min_version,
                                    std::string key = kDefaultKey) {
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

// Runs the hook's After stage with `metadata` for flag `kFlagKey`.
void RunAfter(CodeReadinessHook& hook, FlagMetadata metadata) {
  hook.After(MakeHookContext(), MakeDetails(std::move(metadata)),
             openfeature::HookHints{});
}

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

TEST(CodeReadinessHookCreateTest, InvalidCurrentVersionSucceedsInLenientMode) {
  EXPECT_THAT(CodeReadinessHook::Create("not-a-version"), IsOk());
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
              IsOk());
}

// Runs every version-check test in both modes, because the
// outcome of a successful comparison must not depend on `strict_validation`.
class CodeReadinessHookVersionCheckTest
    : public ::testing::TestWithParam<bool> {
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

  EXPECT_NO_THROW(RunAfter(*hook, MetadataWithMinVersion("1.2.3")));
}

TEST_P(CodeReadinessHookVersionCheckTest, PassesWhenCurrentIsHigher) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHookOrDie("2.0.0", Options());
  ASSERT_THAT(hook, NotNull());

  EXPECT_NO_THROW(RunAfter(*hook, MetadataWithMinVersion("1.9.9")));
}

TEST_P(CodeReadinessHookVersionCheckTest, PassesWithCLNumbers) {
  std::shared_ptr<CodeReadinessHook> hook =
      CreateHookOrDie("527319014", Options());
  ASSERT_THAT(hook, NotNull());

  EXPECT_NO_THROW(RunAfter(*hook, MetadataWithMinVersion("527319013")));
}

TEST_P(CodeReadinessHookVersionCheckTest, ThrowsWhenCurrentIsLower) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHookOrDie("1.2.3", Options());
  ASSERT_THAT(hook, NotNull());

  EXPECT_THAT([&] { RunAfter(*hook, MetadataWithMinVersion("1.3.0")); },
              ThrowsMessage<std::runtime_error>(AllOf(
                  HasSubstr("current version: \"1.2.3\""),
                  HasSubstr("required minimum version: \"1.3.0\""),
                  HasSubstr("flag \"my-flag\""), HasSubstr("check failed"))));
}

TEST_P(CodeReadinessHookVersionCheckTest, ThrowsWhenCurrentIsPreRelease) {
  std::shared_ptr<CodeReadinessHook> hook =
      CreateHookOrDie("1.0.0-rc.1", Options());
  ASSERT_THAT(hook, NotNull());

  EXPECT_THROW(RunAfter(*hook, MetadataWithMinVersion("1.0.0")),
               std::runtime_error);
}

TEST_P(CodeReadinessHookVersionCheckTest, ThrowsWithLowerCLNumber) {
  std::shared_ptr<CodeReadinessHook> hook =
      CreateHookOrDie("527319012", Options());
  ASSERT_THAT(hook, NotNull());

  EXPECT_THROW(RunAfter(*hook, MetadataWithMinVersion("527319013")),
               std::runtime_error);
}

TEST_P(CodeReadinessHookVersionCheckTest, IgnoresUnrelatedMetadata) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHookOrDie("1.2.3", Options());
  ASSERT_THAT(hook, NotNull());
  FlagMetadata metadata = MetadataWithMinVersion("1.0.0");
  metadata.data["owner"] = std::string("team-a");
  metadata.data["priority"] = int64_t{5};

  EXPECT_NO_THROW(RunAfter(*hook, std::move(metadata)));
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
  FlagMetadata bool_value;
  bool_value.data[kDefaultKey] = true;
  FlagMetadata int_value;
  int_value.data[kDefaultKey] = int64_t{527319013};
  FlagMetadata double_value;
  double_value.data[kDefaultKey] = 1.5;
  FlagMetadata other_key;
  other_key.data["otherKey"] = std::string("1.0.0");

  return {
      {"EmptyMetadata", "1.2.3", FlagMetadata{},
       "Flag metadata is empty for flag \"my-flag\""},
      {"MissingKey", "1.2.3", other_key,
       "key \"minCodeVersion\" missing in flag's \"my-flag\" metadata"},
      {"BoolValue", "1.2.3", bool_value,
       "metadata \"minCodeVersion\" is not a string for flag \"my-flag\""},
      {"IntegerValue", "1.2.3", int_value,
       "metadata \"minCodeVersion\" is not a string for flag \"my-flag\""},
      {"DoubleValue", "1.2.3", double_value,
       "metadata \"minCodeVersion\" is not a string for flag \"my-flag\""},
      {"EmptyValue", "1.2.3", MetadataWithMinVersion(""),
       "metadata \"minCodeVersion\" value is empty for flag \"my-flag\""},
      {"InvalidRequiredVersion", "1.2.3", MetadataWithMinVersion("1.x"),
       "invalid version values current: \"1.2.3\" - minimum: \"1.x\""},
      {"InvalidCurrentVersion", "not-a-version",
       MetadataWithMinVersion("1.0.0"), "invalid current semver"},
  };
}

std::string InvalidDataCaseName(
    const ::testing::TestParamInfo<InvalidDataCase>& info) {
  return info.param.test_name;
}

class CodeReadinessHookLenientTest
    : public ::testing::TestWithParam<InvalidDataCase> {};

TEST_P(CodeReadinessHookLenientTest, SkipsValidation) {
  const InvalidDataCase& test_case = GetParam();
  std::shared_ptr<CodeReadinessHook> hook =
      CreateHookOrDie(test_case.current_version);
  ASSERT_THAT(hook, NotNull());

  EXPECT_NO_THROW(RunAfter(*hook, test_case.metadata));
}

INSTANTIATE_TEST_SUITE_P(InvalidData, CodeReadinessHookLenientTest,
                         ::testing::ValuesIn(InvalidDataCases()),
                         InvalidDataCaseName);

class CodeReadinessHookStrictTest
    : public ::testing::TestWithParam<InvalidDataCase> {};

TEST_P(CodeReadinessHookStrictTest, ThrowsWithDescriptiveMessage) {
  const InvalidDataCase& test_case = GetParam();
  if (test_case.test_name == "InvalidCurrentVersion") {
    GTEST_SKIP() << "Create fails for an invalid current version in strict "
                    "mode; covered by CodeReadinessHookCreateTest.";
  }
  std::shared_ptr<CodeReadinessHook> hook =
      CreateHookOrDie(test_case.current_version, StrictOptions());
  ASSERT_THAT(hook, NotNull());

  EXPECT_THAT(
      [&] { RunAfter(*hook, test_case.metadata); },
      ThrowsMessage<std::runtime_error>(HasSubstr(test_case.strict_message)));
}

INSTANTIATE_TEST_SUITE_P(InvalidData, CodeReadinessHookStrictTest,
                         ::testing::ValuesIn(InvalidDataCases()),
                         InvalidDataCaseName);

TEST(CodeReadinessHookMetadataKeyTest, ReadsVersionFromCustomKey) {
  CodeReadinessHookOptions options;
  options.min_version_metadata_key = "requiredAppVersion";
  std::shared_ptr<CodeReadinessHook> hook =
      CreateHookOrDie("1.0.0", std::move(options));
  ASSERT_THAT(hook, NotNull());

  EXPECT_THROW(
      RunAfter(*hook, MetadataWithMinVersion("2.0.0", "requiredAppVersion")),
      std::runtime_error);
}

TEST(CodeReadinessHookMetadataKeyTest, IgnoresDefaultKeyWhenCustomKeyIsSet) {
  CodeReadinessHookOptions options = StrictOptions();
  options.min_version_metadata_key = "requiredAppVersion";
  std::shared_ptr<CodeReadinessHook> hook =
      CreateHookOrDie("1.0.0", std::move(options));
  ASSERT_THAT(hook, NotNull());

  // "minCodeVersion" would fail the check, but the hook must not read it.
  EXPECT_THAT(
      [&] { RunAfter(*hook, MetadataWithMinVersion("2.0.0", kDefaultKey)); },
      ThrowsMessage<std::runtime_error>(
          HasSubstr("key \"requiredAppVersion\" missing")));
}

class CodeReadinessHookCustomComparatorTest : public ::testing::Test {
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

  EXPECT_NO_THROW(RunAfter(*hook, MetadataWithMinVersion("build-40")));
}

TEST_F(CodeReadinessHookCustomComparatorTest, ThrowsWhenCompareReturnsFalse) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHook(/*strict=*/false);
  ASSERT_THAT(hook, NotNull());
  EXPECT_CALL(*comparator_, Compare("build-50")).WillOnce(Return(false));

  EXPECT_THAT([&] { RunAfter(*hook, MetadataWithMinVersion("build-50")); },
              ThrowsMessage<std::runtime_error>(
                  AllOf(HasSubstr("current version: \"build-42\""),
                        HasSubstr("required minimum version: \"build-50\""))));
}

TEST_F(CodeReadinessHookCustomComparatorTest, SkipsOnCompareErrorWhenLenient) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHook(/*strict=*/false);
  ASSERT_THAT(hook, NotNull());
  EXPECT_CALL(*comparator_, Compare(_))
      .WillOnce(Return(absl::InvalidArgumentError("bad build")));

  EXPECT_NO_THROW(RunAfter(*hook, MetadataWithMinVersion("build-x")));
}

TEST_F(CodeReadinessHookCustomComparatorTest, ThrowsOnCompareErrorWhenStrict) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHook(/*strict=*/true);
  ASSERT_THAT(hook, NotNull());
  EXPECT_CALL(*comparator_, Compare(_))
      .WillOnce(Return(absl::InvalidArgumentError("bad build")));

  EXPECT_THAT([&] { RunAfter(*hook, MetadataWithMinVersion("build-x")); },
              ThrowsMessage<std::runtime_error>(HasSubstr("bad build")));
}

TEST_F(CodeReadinessHookCustomComparatorTest,
       DoesNotCallCompareWhenMetadataIsMissing) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHook(/*strict=*/false);
  ASSERT_THAT(hook, NotNull());
  EXPECT_NO_THROW(RunAfter(*hook, FlagMetadata{}));
  EXPECT_NO_THROW(RunAfter(*hook, MetadataWithMinVersion("")));
}

TEST(CodeReadinessHookIntegrationTest, WorksThroughGeneralHookInterface) {
  std::shared_ptr<openfeature::GeneralHook> hook = CreateHookOrDie("1.0.0");
  ASSERT_THAT(hook, NotNull());

  EXPECT_THROW(hook->After(MakeHookContext(),
                           MakeDetails(MetadataWithMinVersion("2.0.0")),
                           openfeature::HookHints{}),
               std::runtime_error);
}

TEST(CodeReadinessHookIntegrationTest, WorksForNonBooleanFlags) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHookOrDie("1.0.0");
  ASSERT_THAT(hook, NotNull());
  openfeature::StringHookContext ctx(
      "string-flag", openfeature::FlagValueType::kString, "default",
      openfeature::EvaluationContext::Builder().build(),
      openfeature::Metadata{"client"}, openfeature::Metadata{"provider"},
      std::make_shared<openfeature::HookData>());
  openfeature::StringFlagEvaluationDetails details(
      "string-flag", "value", openfeature::Reason::kStatic, "v1",
      MetadataWithMinVersion("2.0.0"));

  EXPECT_THAT(
      [&] { hook->After(ctx, details, openfeature::HookHints{}); },
      ThrowsMessage<std::runtime_error>(HasSubstr("flag \"string-flag\"")));
}

TEST(CodeReadinessHookIntegrationTest, CanBeReusedAcrossEvaluations) {
  std::shared_ptr<CodeReadinessHook> hook = CreateHookOrDie("1.5.0");
  ASSERT_THAT(hook, NotNull());

  EXPECT_NO_THROW(RunAfter(*hook, MetadataWithMinVersion("1.0.0")));
  EXPECT_THROW(RunAfter(*hook, MetadataWithMinVersion("2.0.0")),
               std::runtime_error);
  EXPECT_NO_THROW(RunAfter(*hook, MetadataWithMinVersion("1.5.0")));
}

}  // namespace codereadiness
