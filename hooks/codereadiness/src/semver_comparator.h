#ifndef CPP_SDK_CONTRIB_HOOKS_CODEREADINESS_SEMVER_COMPARATOR_H_
#define CPP_SDK_CONTRIB_HOOKS_CODEREADINESS_SEMVER_COMPARATOR_H_

#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "codereadiness/semver.h"
#include "codereadiness/version_comparator.h"

namespace codereadiness {

// Default `VersionComparator`: interprets both versions as SemVer 2.0.0 (see
// `SemVer::Parse` for accepted formats) and reports whether the current
// version has precedence >= the required version.
class SemVerComparator final : public VersionComparator {
 public:
  absl::Status Initialize(std::string_view current_version) override;

  absl::StatusOr<bool> Compare(
      std::string_view required_version) const override;

 private:
  SemVer parsed_current_;
};

}  // namespace codereadiness

#endif  // CPP_SDK_CONTRIB_HOOKS_CODEREADINESS_SEMVER_COMPARATOR_H_
