#ifndef CPP_SDK_CONTRIB_HOOKS_CODEREADINESS_SEMVER_H_
#define CPP_SDK_CONTRIB_HOOKS_CODEREADINESS_SEMVER_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"

namespace codereadiness {

// Represents a Semantic Version (SemVer 2.0.0) for comparison.
// Parsing follows golang.org/x/mod/semver: optional leading 'v'/'V', MAJOR and
// MAJOR.MINOR shorthands (without pre-release/build), strict identifiers.
class SemVer {
 public:
  // Parses `text` as a SemVer 2.0.0 version. Accepts an optional leading
  // 'v'/'V' and the MAJOR / MAJOR.MINOR shorthands (missing parts default to
  // 0; shorthands cannot have pre-release or build metadata). Build metadata is
  // validated but discarded. Returns `kInvalidArgument` on malformed input.
  static absl::StatusOr<SemVer> Parse(std::string_view text);
  explicit SemVer(uint64_t major = 0, uint64_t minor = 0, uint64_t patch = 0,
                  std::vector<std::string> pre_release = {})
      : major_(major),
        minor_(minor),
        patch_(patch),
        pre_release_(std::move(pre_release)) {}

  uint64_t GetMajor() const { return major_; }
  uint64_t GetMinor() const { return minor_; }
  uint64_t GetPatch() const { return patch_; }
  const std::vector<std::string>& GetPreRelease() const { return pre_release_; }

  // SemVer 2.0.0 precedence. Returns -1 if this < other, 0 if equal, 1 if this
  // > other.
  int Compare(const SemVer& other) const;

 private:
  uint64_t major_;
  uint64_t minor_;
  uint64_t patch_;
  std::vector<std::string> pre_release_;
};

}  // namespace codereadiness

#endif  // CPP_SDK_CONTRIB_HOOKS_CODEREADINESS_SEMVER_H_
