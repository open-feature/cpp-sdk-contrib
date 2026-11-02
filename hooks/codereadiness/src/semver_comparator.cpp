#include "codereadiness/semver_comparator.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "codereadiness/semver.h"

namespace codereadiness {

absl::Status SemVerComparator::Initialize(std::string_view current_version) {
  current_version_ = std::string(current_version);
  absl::StatusOr<SemVer> parsed = SemVer::Parse(current_version);
  if (!parsed.ok()) {
    parsed_current_.reset();
    return absl::InvalidArgumentError(
        absl::StrCat("invalid current semver: \"", current_version,
                     "\": ", parsed.status().message()));
  }
  parsed_current_ = *std::move(parsed);
  return absl::OkStatus();
}

absl::StatusOr<bool> SemVerComparator::Compare(
    std::string_view required_version) const {
  if (!parsed_current_.has_value()) {
    return absl::FailedPreconditionError(
        absl::StrCat("invalid current semver: \"", current_version_, "\""));
  }

  absl::StatusOr<SemVer> required = SemVer::Parse(required_version);
  if (!required.ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid required semver: \"", required_version,
                     "\": ", required.status().message()));
  }

  return parsed_current_->Compare(*required) >= 0;
}

}  // namespace codereadiness
