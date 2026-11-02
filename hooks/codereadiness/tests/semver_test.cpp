#include "codereadiness/semver.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"

namespace codereadiness {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

// Parses `text`, failing the test if it is not a valid SemVer.
SemVer ParseOrDie(std::string_view text) {
  absl::StatusOr<SemVer> version = SemVer::Parse(text);
  EXPECT_THAT(version, IsOk()) << "for input \"" << text << "\"";
  return version.ok() ? *version : SemVer();
}

TEST(SemVerTest, DefaultConstructorIsZeroVersion) {
  SemVer version;

  EXPECT_EQ(version.GetMajor(), 0);
  EXPECT_EQ(version.GetMinor(), 0);
  EXPECT_EQ(version.GetPatch(), 0);
  EXPECT_THAT(version.GetPreRelease(), IsEmpty());
}

TEST(SemVerTest, ConstructorSetsAllFields) {
  SemVer version(1, 2, 3, {"alpha", "1"});

  EXPECT_EQ(version.GetMajor(), 1);
  EXPECT_EQ(version.GetMinor(), 2);
  EXPECT_EQ(version.GetPatch(), 3);
  EXPECT_THAT(version.GetPreRelease(), ElementsAre("alpha", "1"));
}

TEST(SemVerParseTest, ParsesFullVersion) {
  SemVer version = ParseOrDie("1.2.3");

  EXPECT_EQ(version.GetMajor(), 1);
  EXPECT_EQ(version.GetMinor(), 2);
  EXPECT_EQ(version.GetPatch(), 3);
  EXPECT_THAT(version.GetPreRelease(), IsEmpty());
}

TEST(SemVerParseTest, ParsesZeroVersion) {
  SemVer version = ParseOrDie("0.0.0");

  EXPECT_EQ(version.GetMajor(), 0);
  EXPECT_EQ(version.GetMinor(), 0);
  EXPECT_EQ(version.GetPatch(), 0);
}

TEST(SemVerParseTest, StripsLowercaseVPrefix) {
  SemVer version = ParseOrDie("v1.2.3");

  EXPECT_EQ(version.GetMajor(), 1);
  EXPECT_EQ(version.GetMinor(), 2);
  EXPECT_EQ(version.GetPatch(), 3);
}

TEST(SemVerParseTest, StripsUppercaseVPrefix) {
  SemVer version = ParseOrDie("V1.2.3");

  EXPECT_EQ(version.GetMajor(), 1);
  EXPECT_EQ(version.GetMinor(), 2);
  EXPECT_EQ(version.GetPatch(), 3);
}

TEST(SemVerParseTest, MajorOnlyShorthandDefaultsMinorAndPatchToZero) {
  SemVer version = ParseOrDie("527319013");

  EXPECT_EQ(version.GetMajor(), 527319013);
  EXPECT_EQ(version.GetMinor(), 0);
  EXPECT_EQ(version.GetPatch(), 0);
}

TEST(SemVerParseTest, MajorMinorShorthandDefaultsPatchToZero) {
  SemVer version = ParseOrDie("v7.4");

  EXPECT_EQ(version.GetMajor(), 7);
  EXPECT_EQ(version.GetMinor(), 4);
  EXPECT_EQ(version.GetPatch(), 0);
}

TEST(SemVerParseTest, ParsesMaxUint64Component) {
  SemVer version = ParseOrDie("18446744073709551615.0.0");

  EXPECT_EQ(version.GetMajor(), UINT64_MAX);
}

TEST(SemVerParseTest, ParsesPreReleaseIdentifiers) {
  SemVer version = ParseOrDie("1.0.0-alpha.1.x-y");

  EXPECT_THAT(version.GetPreRelease(), ElementsAre("alpha", "1", "x-y"));
}

TEST(SemVerParseTest, PreReleaseMayStartWithHyphen) {
  SemVer version = ParseOrDie("1.2.3--a");

  EXPECT_THAT(version.GetPreRelease(), ElementsAre("-a"));
}

TEST(SemVerParseTest, PreReleaseAlphanumericWithLeadingZeroIsAllowed) {
  // The leading zero rule applies only to purely numeric identifiers.
  SemVer version = ParseOrDie("1.2.3-0a");

  EXPECT_THAT(version.GetPreRelease(), ElementsAre("0a"));
}

TEST(SemVerParseTest, PreReleaseZeroIsAllowed) {
  SemVer version = ParseOrDie("1.2.3-0");

  EXPECT_THAT(version.GetPreRelease(), ElementsAre("0"));
}

TEST(SemVerParseTest, BuildMetadataIsValidatedAndDropped) {
  SemVer version = ParseOrDie("1.2.3+build.5");

  EXPECT_EQ(version.GetMajor(), 1);
  EXPECT_EQ(version.GetMinor(), 2);
  EXPECT_EQ(version.GetPatch(), 3);
  EXPECT_THAT(version.GetPreRelease(), IsEmpty());
}

TEST(SemVerParseTest, BuildMetadataNumericWithLeadingZeroIsAllowed) {
  EXPECT_THAT(SemVer::Parse("1.2.3+build.01"), IsOk());
}

TEST(SemVerParseTest, ParsesPreReleaseAndBuildMetadata) {
  SemVer version = ParseOrDie("1.0.0-rc.1+sha.5114f85");

  EXPECT_THAT(version.GetPreRelease(), ElementsAre("rc", "1"));
}

// Build metadata is split off before looking for the pre-release '-'.
TEST(SemVerParseTest, HyphenInBuildMetadataIsNotPreRelease) {
  SemVer version = ParseOrDie("1.2.3+build-1");

  EXPECT_THAT(version.GetPreRelease(), IsEmpty());
}

struct InvalidParseCase {
  std::string test_name;
  std::string input;
  std::string expected_message;
};

class SemVerParseInvalidTest
    : public ::testing::TestWithParam<InvalidParseCase> {};

TEST_P(SemVerParseInvalidTest, ReturnsInvalidArgument) {
  const InvalidParseCase& test_case = GetParam();

  EXPECT_THAT(SemVer::Parse(test_case.input),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(test_case.expected_message)));
}

INSTANTIATE_TEST_SUITE_P(
    InvalidInputs, SemVerParseInvalidTest,
    ::testing::ValuesIn(std::vector<InvalidParseCase>{
        // Empty or missing core.
        {"Empty", "", "Invalid SemVer Major digits"},
        {"OnlyVPrefix", "v", "Invalid SemVer Major digits"},
        {"DoubleVPrefix", "vv1.2.3", "Invalid SemVer Major digits"},
        {"EmptyMinor", "1..3", "Invalid SemVer Minor digits"},
        {"TrailingDot", "1.2.", "Invalid SemVer Patch digits"},
        {"TooManyComponents", "1.2.3.4", "Invalid SemVer core"},
        // Non-digit characters in the core.
        {"LeadingWhitespace", " 1.2.3", "Invalid SemVer Major digits"},
        {"TrailingWhitespace", "1.2.3 ", "Invalid SemVer Patch digits"},
        // The first '-' starts the pre-release, so the core here is "1.".
        {"NegativeNumber", "1.-2.3", "Shorthand SemVer"},
        {"PlusSign", "1.2.+3", "Invalid SemVer Patch digits"},
        {"Letters", "1.x.3", "Invalid SemVer Minor digits"},
        // Leading zeros in the core.
        {"MajorLeadingZero", "01.2.3", "Major version MUST NOT contain"},
        {"MinorLeadingZero", "1.02.3", "Minor version MUST NOT contain"},
        {"PatchLeadingZero", "1.2.03", "Patch version MUST NOT contain"},
        // Overflow.
        {"MajorOverflow", "18446744073709551616", "Major is too large"},
        // Shorthands cannot carry pre-release or build metadata.
        {"MajorShorthandWithPreRelease", "1-alpha", "Shorthand SemVer"},
        {"MinorShorthandWithPreRelease", "1.2-rc1", "Shorthand SemVer"},
        {"MinorShorthandWithBuild", "1.2+build", "Shorthand SemVer"},
        {"OnlyBuild", "+1.2.3", "Shorthand SemVer"},
        // Pre-release identifiers.
        {"EmptyPreRelease", "1.2.3-", "Empty pre-release identifier"},
        {"EmptyPreReleaseIdentifier", "1.2.3-a..b",
         "Empty pre-release identifier"},
        {"TrailingDotInPreRelease", "1.2.3-a.", "Empty pre-release identifier"},
        {"InvalidCharInPreRelease", "1.2.3-alpha_1",
         "Invalid character in pre-release identifier"},
        {"NonAsciiInPreRelease", "1.2.3-\xC3\xBC",
         "Invalid character in pre-release identifier"},
        {"NumericPreReleaseLeadingZero", "1.2.3-01",
         "Numeric pre-release identifiers MUST NOT contain leading zeros"},
        // Build metadata identifiers.
        {"EmptyBuild", "1.2.3+", "Empty build identifier"},
        {"EmptyBuildIdentifier", "1.2.3+a..b", "Empty build identifier"},
        {"SecondPlusInBuild", "1.2.3+a+b",
         "Invalid character in build identifier"},
        {"InvalidCharInBuild", "1.2.3+build_1",
         "Invalid character in build identifier"},
    }),
    [](const ::testing::TestParamInfo<InvalidParseCase>& info) {
      return info.param.test_name;
    });

struct CompareCase {
  std::string test_name;
  std::string lhs;
  std::string rhs;
  int expected;  // Expected result of lhs.Compare(rhs).
};

class SemVerCompareTest : public ::testing::TestWithParam<CompareCase> {};

TEST_P(SemVerCompareTest, ReturnsExpectedOrdering) {
  const CompareCase& test_case = GetParam();
  SemVer lhs = ParseOrDie(test_case.lhs);
  SemVer rhs = ParseOrDie(test_case.rhs);

  EXPECT_EQ(lhs.Compare(rhs), test_case.expected)
      << test_case.lhs << " vs " << test_case.rhs;
}

TEST_P(SemVerCompareTest, IsAntisymmetric) {
  const CompareCase& test_case = GetParam();
  SemVer lhs = ParseOrDie(test_case.lhs);
  SemVer rhs = ParseOrDie(test_case.rhs);

  EXPECT_EQ(rhs.Compare(lhs), -test_case.expected)
      << test_case.rhs << " vs " << test_case.lhs;
}

INSTANTIATE_TEST_SUITE_P(
    Precedence, SemVerCompareTest,
    ::testing::ValuesIn(std::vector<CompareCase>{
        // Equality.
        {"Equal", "1.2.3", "1.2.3", 0},
        {"EqualWithVPrefix", "v1.2.3", "1.2.3", 0},
        {"MajorShorthandEqualsFull", "1", "1.0.0", 0},
        {"MinorShorthandEqualsFull", "1.2", "1.2.0", 0},
        {"EqualPreRelease", "1.2.3-rc.1", "1.2.3-rc.1", 0},
        {"BuildMetadataIgnored", "1.2.3+a", "1.2.3+b", 0},
        {"BuildMetadataIgnoredVsNone", "1.2.3+a", "1.2.3", 0},
        // Core components, compared numerically (not lexicographically).
        {"MajorLess", "1.9.9", "2.0.0", -1},
        {"MinorLess", "1.2.9", "1.3.0", -1},
        {"PatchLess", "1.2.3", "1.2.4", -1},
        {"MajorNumericNotLexicographic", "2.0.0", "10.0.0", -1},
        {"MinorNumericNotLexicographic", "1.2.0", "1.10.0", -1},
        {"PatchNumericNotLexicographic", "1.0.9", "1.0.10", -1},
        {"CLNumbers", "527319013", "527319014", -1},
        {"MaxUint64", "18446744073709551614", "18446744073709551615", -1},
        // Pre-release has lower precedence than the release.
        {"PreReleaseLessThanRelease", "1.0.0-alpha", "1.0.0", -1},
        {"PreReleaseOfHigherPatch", "1.0.0", "1.0.1-alpha", -1},
        // Pre-release precedence (examples from semver.org, item 11).
        {"AlphaLessThanAlpha1", "1.0.0-alpha", "1.0.0-alpha.1", -1},
        {"Alpha1LessThanAlphaBeta", "1.0.0-alpha.1", "1.0.0-alpha.beta", -1},
        {"AlphaBetaLessThanBeta", "1.0.0-alpha.beta", "1.0.0-beta", -1},
        {"BetaLessThanBeta2", "1.0.0-beta", "1.0.0-beta.2", -1},
        {"Beta2LessThanBeta11", "1.0.0-beta.2", "1.0.0-beta.11", -1},
        {"Beta11LessThanRc1", "1.0.0-beta.11", "1.0.0-rc.1", -1},
        // Pre-release identifier rules.
        {"NumericLessThanAlphanumeric", "1.0.0-1", "1.0.0-a", -1},
        {"NumericComparedNumerically", "1.0.0-9", "1.0.0-10", -1},
        {"AlphanumericComparedInAscii", "1.0.0-Beta", "1.0.0-alpha", -1},
        {"HyphenSortsBeforeLetters", "1.0.0--", "1.0.0-a", -1},
        {"LongerPreReleaseWins", "1.0.0-a.b", "1.0.0-a.b.c", -1},
        {"ArbitraryLengthNumericIdentifier", "1.0.0-99999999999999999999",
         "1.0.0-100000000000000000000", -1},
    }),
    [](const ::testing::TestParamInfo<CompareCase>& info) {
      return info.param.test_name;
    });

TEST(SemVerCompareTest, IsReflexive) {
  for (std::string_view text :
       {"0.0.0", "1.2.3", "1.0.0-alpha.1", "527319013"}) {
    SemVer version = ParseOrDie(text);
    EXPECT_EQ(version.Compare(version), 0) << text;
  }
}

TEST(SemVerCompareTest, SemVerOrgPrecedenceChainIsStrictlyIncreasing) {
  const std::vector<std::string_view> ordered = {
      "1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta",
      "1.0.0-beta",  "1.0.0-beta.2",  "1.0.0-beta.11",
      "1.0.0-rc.1",  "1.0.0",         "1.0.1",
      "1.1.0",       "2.0.0",
  };

  for (size_t i = 0; i < ordered.size(); ++i) {
    for (size_t j = i + 1; j < ordered.size(); ++j) {
      EXPECT_EQ(ParseOrDie(ordered[i]).Compare(ParseOrDie(ordered[j])), -1)
          << ordered[i] << " should be lower than " << ordered[j];
    }
  }
}

TEST(SemVerCompareTest, ParsedVersionMatchesConstructedVersion) {
  EXPECT_EQ(
      ParseOrDie("v1.2.3-rc.1+build").Compare(SemVer(1, 2, 3, {"rc", "1"})), 0);
  EXPECT_THAT(SemVer::Parse("1.2.3-rc.01"), Not(IsOk()));
}

}  // namespace codereadiness
