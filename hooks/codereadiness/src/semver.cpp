#include "codereadiness/semver.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"

namespace codereadiness {
namespace {

// Checks if a string has a leading zero (and is not just "0").
bool HasLeadingZero(std::string_view str) {
  return str.size() > 1 && str[0] == '0';
}

// Returns true if the string is non-empty and consists only of ASCII digits.
bool IsNumeric(std::string_view str) {
  return !str.empty() && std::all_of(str.begin(), str.end(), [](char chr) {
    return absl::ascii_isdigit(static_cast<unsigned char>(chr));
  });
}

// SemVer identifier characters are [0-9A-Za-z-] (ASCII only).
bool IsIdentifierChar(char chr) {
  return absl::ascii_isalnum(static_cast<unsigned char>(chr)) || chr == '-';
}

// Parses number according to SemVer 2.0.0 specification.
absl::StatusOr<uint64_t> ParseSemVerNum(std::string_view num_str,
                                        std::string_view name) {
  if (!IsNumeric(num_str)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid SemVer ", name, " digits: \"", num_str, "\""));
  }
  if (HasLeadingZero(num_str)) {
    return absl::InvalidArgumentError(absl::StrCat(
        name, " version MUST NOT contain leading zeros: ", num_str));
  }
  uint64_t value = 0;
  if (!absl::SimpleAtoi(num_str, &value)) {
    return absl::InvalidArgumentError(
        absl::StrCat("SemVer ", name, " is too large: ", num_str));
  }
  return value;
}

// Validates a dot-separated list of identifiers (pre-release or build).
absl::Status ValidateIdentifiers(std::string_view identifiers,
                                 std::string_view kind,
                                 bool reject_numeric_leading_zero) {
  for (std::string_view ident : absl::StrSplit(identifiers, '.')) {
    if (ident.empty()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Empty ", kind, " identifier"));
    }
    if (!std::all_of(ident.begin(), ident.end(), IsIdentifierChar)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid character in ", kind, " identifier: \"", ident, "\""));
    }
    if (reject_numeric_leading_zero && IsNumeric(ident) &&
        HasLeadingZero(ident)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Numeric ", kind, " identifiers MUST NOT contain leading zeros"));
    }
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<SemVer> SemVer::Parse(std::string_view text) {
  if (!text.empty() && (text[0] == 'v' || text[0] == 'V')) {
    text.remove_prefix(1);
  }

  // 1. Split off build metadata at the first '+' and validate it
  bool has_build = false;
  if (size_t plus = text.find('+'); plus != std::string_view::npos) {
    has_build = true;
    std::string_view build = text.substr(plus + 1);
    text = text.substr(0, plus);
    if (absl::Status status = ValidateIdentifiers(build, "build", false);
        !status.ok()) {
      return status;
    }
  }

  // 2. Separate core and pre-release at the first '-'.
  bool has_pre_release = false;
  std::string_view pre_release_text;
  if (size_t dash = text.find('-'); dash != std::string_view::npos) {
    has_pre_release = true;
    pre_release_text = text.substr(dash + 1);
    text = text.substr(0, dash);
  }
  std::string_view core = text;

  // 3. Parse core components (major[.minor[.patch]]).
  std::vector<std::string_view> core_parts = absl::StrSplit(core, '.');
  if (core_parts.empty() || core_parts.size() > 3) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid SemVer core: ", core));
  }
  // 4. Validate that shorthand versions do not have pre-release or build
  // metadata.
  if (core_parts.size() < 3 && (has_pre_release || has_build)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Shorthand SemVer \"", core,
                     "\" cannot have pre-release or build metadata"));
  }

  absl::StatusOr<uint64_t> major = ParseSemVerNum(core_parts[0], "Major");
  if (!major.ok()) return major.status();

  uint64_t minor = 0;
  if (core_parts.size() >= 2) {
    absl::StatusOr<uint64_t> minor_res = ParseSemVerNum(core_parts[1], "Minor");
    if (!minor_res.ok()) return minor_res.status();
    minor = *minor_res;
  }

  uint64_t patch = 0;
  if (core_parts.size() == 3) {
    absl::StatusOr<uint64_t> patch_res = ParseSemVerNum(core_parts[2], "Patch");
    if (!patch_res.ok()) return patch_res.status();
    patch = *patch_res;
  }

  // 4. Parse pre-release identifiers.
  std::vector<std::string> pre_release;
  if (has_pre_release) {
    if (absl::Status status =
            ValidateIdentifiers(pre_release_text, "pre-release", true);
        !status.ok()) {
      return status;
    }
    for (std::string_view ident : absl::StrSplit(pre_release_text, '.')) {
      pre_release.emplace_back(ident);
    }
  }

  return SemVer(*major, minor, patch, std::move(pre_release));
}

int SemVer::Compare(const SemVer& other) const {
  if (major_ != other.major_) return major_ < other.major_ ? -1 : 1;
  if (minor_ != other.minor_) return minor_ < other.minor_ ? -1 : 1;
  if (patch_ != other.patch_) return patch_ < other.patch_ ? -1 : 1;

  // A normal version has higher precedence than a pre-release version
  if (pre_release_.empty() && !other.pre_release_.empty()) return 1;
  if (!pre_release_.empty() && other.pre_release_.empty()) return -1;
  if (pre_release_.empty() && other.pre_release_.empty()) return 0;

  size_t len = std::min(pre_release_.size(), other.pre_release_.size());
  for (size_t i = 0; i < len; ++i) {
    const std::string& lhs_part = pre_release_[i];
    const std::string& rhs_part = other.pre_release_[i];

    // Numeric identifiers have lower precedence than non-numeric identifiers.
    const bool lhs_is_num = IsNumeric(lhs_part);
    const bool rhs_is_num = IsNumeric(rhs_part);

    if (lhs_is_num && rhs_is_num) {
      // Compare numerically by length first, then lexicographically.
      // This supports arbitrary precision integers without overflow.
      if (lhs_part.length() != rhs_part.length()) {
        return lhs_part.length() < rhs_part.length() ? -1 : 1;
      }
      if (lhs_part != rhs_part) return lhs_part < rhs_part ? -1 : 1;
    } else if (lhs_is_num && !rhs_is_num) {
      return -1;
    } else if (!lhs_is_num && rhs_is_num) {
      return 1;
    } else {
      // Non-numeric identifiers are compared lexicographically in ASCII sort
      // order.
      if (lhs_part != rhs_part) return lhs_part < rhs_part ? -1 : 1;
    }
  }

  // A larger set of pre-release fields has a higher precedence than a smaller
  // set.
  if (pre_release_.size() != other.pre_release_.size()) {
    return pre_release_.size() < other.pre_release_.size() ? -1 : 1;
  }

  return 0;
}

}  // namespace codereadiness
