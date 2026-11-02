#ifndef CPP_SDK_CONTRIB_HOOKS_CODEREADINESS_VERSION_COMPARATOR_H_
#define CPP_SDK_CONTRIB_HOOKS_CODEREADINESS_VERSION_COMPARATOR_H_

#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace codereadiness {

// Compares the binary's current version against a flag's required
// minimum version.
class VersionComparator {
 public:
  VersionComparator() = default;
  VersionComparator(const VersionComparator&) = delete;
  VersionComparator& operator=(const VersionComparator&) = delete;
  virtual ~VersionComparator() = default;

  // Called once when the hook is created, with the binary's current version.
  virtual absl::Status Initialize(std::string_view current_version) = 0;

  // Returns true if current version >= required_version, or an error if
  // either version can't be parsed. Must be thread-safe since it's called
  // concurrently from different flag evaluations.
  virtual absl::StatusOr<bool> Compare(
      std::string_view required_version) const = 0;
};

}  // namespace codereadiness

#endif  // CPP_SDK_CONTRIB_HOOKS_CODEREADINESS_VERSION_COMPARATOR_H_
