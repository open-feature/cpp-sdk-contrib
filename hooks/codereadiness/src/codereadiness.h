#ifndef CPP_SDK_CONTRIB_HOOKS_CODEREADINESS_CODEREADINESS_H_
#define CPP_SDK_CONTRIB_HOOKS_CODEREADINESS_CODEREADINESS_H_

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "codereadiness/version_comparator.h"
#include "openfeature/general_flag_evaluation_details.h"
#include "openfeature/general_hook.h"
#include "openfeature/general_hook_context.h"
#include "openfeature/hook_hints.h"

namespace codereadiness {

struct CodeReadinessHookOptions {
  // Compares versions. If null, a `SemVerComparator` is used.
  std::unique_ptr<VersionComparator> comparator;
  // Flag metadata key holding the flag's minimum required code version.
  std::string min_version_metadata_key = "minCodeVersion";
  // If true, malformed or missing version data fails the evaluation;
  // If false, it is logged at VLOG(1) and the check is skipped. A well-formed
  // current version lower than the required one always fails the evaluation.
  bool strict_validation = false;
};

// OpenFeature hook that fails a flag evaluation when the binary's version is
// lower than the minimum version stored in the flag's metadata.
//
// Thread-safe: a single instance may be shared across evaluations.
class CodeReadinessHook final : public openfeature::GeneralHook {
 public:
  // Returns `kInvalidArgument` if `current_version` or the metadata key is
  // empty, or (in strict mode) if the comparator rejects `current_version`.
  static absl::StatusOr<std::shared_ptr<CodeReadinessHook>> Create(
      std::string current_version, CodeReadinessHookOptions options = {});

  // Throws `std::runtime_error` to fail the evaluation.
  void After(const openfeature::GeneralHookContext& ctx,
             const openfeature::GeneralFlagEvaluationDetails& details,
             const openfeature::HookHints& hints) override;

 private:
  CodeReadinessHook(std::string current_version,
                    std::unique_ptr<VersionComparator> comparator,
                    bool strict_validation, std::string metadata_min_ver_key);

  void FailOrSkip(const std::string& message) const;

  const std::string current_version_;
  const std::unique_ptr<const VersionComparator> comparator_;
  const bool strict_validation_;
  const std::string metadata_min_ver_key_;
};

}  // namespace codereadiness

#endif  // CPP_SDK_CONTRIB_HOOKS_CODEREADINESS_CODEREADINESS_H_
