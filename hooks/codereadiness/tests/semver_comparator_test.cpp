#include "codereadiness/semver_comparator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "codereadiness/version_comparator.h"

namespace codereadiness {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Not;

TEST(SemVerComparatorInitializeTest, AcceptsValidVersion) {
  SemVerComparator comparator;

  EXPECT_THAT(comparator.Initialize("1.2.3"), IsOk());
}

TEST(SemVerComparatorInitializeTest, AcceptsVersionsSupportedBySemVerParse) {
  for (const char* version :
       {"v1.2.3", "1", "1.2", "527319013", "1.0.0-rc.1+build.5"}) {
    SemVerComparator comparator;
    EXPECT_THAT(comparator.Initialize(version), IsOk()) << version;
  }
}

TEST(SemVerComparatorInitializeTest, RejectsInvalidVersion) {
  SemVerComparator comparator;

  EXPECT_THAT(comparator.Initialize("not-a-version"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid current semver: \"not-a-version\"")));
}

TEST(SemVerComparatorInitializeTest, RejectsEmptyVersion) {
  SemVerComparator comparator;

  EXPECT_THAT(comparator.Initialize(""),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid current semver: \"\"")));
}

TEST(SemVerComparatorInitializeTest, ErrorIncludesUnderlyingParseError) {
  SemVerComparator comparator;

  EXPECT_THAT(comparator.Initialize("01.2.3"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("\"01.2.3\""),
                             HasSubstr("MUST NOT contain leading zeros"))));
}

TEST(SemVerComparatorCompareTest, FailsWhenRequiredVersionIsInvalid) {
  SemVerComparator comparator;
  ASSERT_THAT(comparator.Initialize("1.2.3"), IsOk());

  EXPECT_THAT(comparator.Compare("1.2.x"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("invalid required semver: \"1.2.x\""),
                             HasSubstr("Invalid SemVer Patch digits"))));
}

TEST(SemVerComparatorCompareTest, FailsWhenRequiredVersionIsEmpty) {
  SemVerComparator comparator;
  ASSERT_THAT(comparator.Initialize("1.2.3"), IsOk());

  EXPECT_THAT(comparator.Compare(""),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid required semver: \"\"")));
}

struct CompareCase {
  std::string test_name;
  std::string current;
  std::string required;
  bool expected;  // `current` >= `required`.
};

class SemVerComparatorResultTest
    : public ::testing::TestWithParam<CompareCase> {};

TEST_P(SemVerComparatorResultTest, ReturnsWhetherCurrentMeetsRequired) {
  const CompareCase& test_case = GetParam();
  SemVerComparator comparator;
  ASSERT_THAT(comparator.Initialize(test_case.current), IsOk());

  EXPECT_THAT(comparator.Compare(test_case.required),
              IsOkAndHolds(test_case.expected))
      << "current: " << test_case.current
      << ", required: " << test_case.required;
}

INSTANTIATE_TEST_SUITE_P(
    Results, SemVerComparatorResultTest,
    ::testing::ValuesIn(std::vector<CompareCase>{
        // Equal versions meet the requirement.
        {"Equal", "1.2.3", "1.2.3", true},
        {"EqualPreRelease", "1.2.3-rc.1", "1.2.3-rc.1", true},
        // Higher current version.
        {"HigherMajor", "2.0.0", "1.9.9", true},
        {"HigherMinor", "1.3.0", "1.2.9", true},
        {"HigherPatch", "1.2.4", "1.2.3", true},
        // Lower current version.
        {"LowerMajor", "1.9.9", "2.0.0", false},
        {"LowerMinor", "1.2.9", "1.3.0", false},
        {"LowerPatch", "1.2.3", "1.2.4", false},
        // Numeric, not lexicographic, comparison.
        {"NumericNotLexicographic", "1.10.0", "1.9.0", true},
        // Pre-release precedence.
        {"ReleaseMeetsItsPreRelease", "1.0.0", "1.0.0-rc.1", true},
        {"PreReleaseDoesNotMeetRelease", "1.0.0-rc.1", "1.0.0", false},
        {"LaterPreRelease", "1.0.0-beta.11", "1.0.0-beta.2", true},
        {"EarlierPreRelease", "1.0.0-alpha", "1.0.0-alpha.1", false},
        // Formats accepted by SemVer::Parse.
        {"VPrefixOnCurrent", "v1.2.3", "1.2.3", true},
        {"VPrefixOnRequired", "1.2.3", "v1.2.4", false},
        {"ShorthandCurrent", "2", "1.9.9", true},
        {"ShorthandRequired", "1.2.3", "1.3", false},
        {"BuildMetadataIgnored", "1.2.3+a", "1.2.3+b", true},
        // CL numbers, as used by EZ Flags.
        {"CLEqual", "527319013", "527319013", true},
        {"CLHigher", "527319014", "527319013", true},
        {"CLLower", "527319012", "527319013", false},
    }),
    [](const ::testing::TestParamInfo<CompareCase>& info) {
      return info.param.test_name;
    });

TEST(SemVerComparatorReinitializeTest, ValidVersionReplacesPreviousVersion) {
  SemVerComparator comparator;
  ASSERT_THAT(comparator.Initialize("1.0.0"), IsOk());
  ASSERT_THAT(comparator.Initialize("2.0.0"), IsOk());

  EXPECT_THAT(comparator.Compare("1.5.0"), IsOkAndHolds(true));
}

TEST(SemVerComparatorReinitializeTest, ValidVersionRecoversFromFailure) {
  SemVerComparator comparator;
  ASSERT_THAT(comparator.Initialize("bad"), Not(IsOk()));
  ASSERT_THAT(comparator.Initialize("1.0.0"), IsOk());

  EXPECT_THAT(comparator.Compare("1.0.0"), IsOkAndHolds(true));
}

TEST(SemVerComparatorInterfaceTest, WorksThroughVersionComparatorInterface) {
  std::unique_ptr<VersionComparator> comparator =
      std::make_unique<SemVerComparator>();
  ASSERT_THAT(comparator->Initialize("1.2.3"), IsOk());

  const VersionComparator& const_comparator = *comparator;
  EXPECT_THAT(const_comparator.Compare("1.2.0"), IsOkAndHolds(true));
  EXPECT_THAT(const_comparator.Compare("1.3.0"), IsOkAndHolds(false));
}

TEST(SemVerComparatorInterfaceTest, CompareIsSafeToCallConcurrently) {
  SemVerComparator comparator;
  ASSERT_THAT(comparator.Initialize("1.5.0"), IsOk());
  const SemVerComparator& const_comparator = comparator;

  constexpr int kNumThreads = 8;
  constexpr int kIterations = 1000;
  std::vector<std::thread> threads;
  std::vector<int> failures(kNumThreads, 0);
  threads.reserve(kNumThreads);
  for (int thread_idx = 0; thread_idx < kNumThreads; ++thread_idx) {
    threads.emplace_back([&const_comparator, &failures, thread_idx] {
      for (int i = 0; i < kIterations; ++i) {
        absl::StatusOr<bool> lower = const_comparator.Compare("1.4.0");
        absl::StatusOr<bool> higher = const_comparator.Compare("1.6.0");
        if (!lower.ok() || !*lower || !higher.ok() || *higher) {
          ++failures[thread_idx];
        }
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }

  for (int thread_idx = 0; thread_idx < kNumThreads; ++thread_idx) {
    EXPECT_EQ(failures[thread_idx], 0) << "thread " << thread_idx;
  }
}

}  // namespace codereadiness
