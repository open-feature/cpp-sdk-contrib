#include "codereadiness/codereadiness.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "codereadiness/semver_comparator.h"
#include "openfeature/flag_metadata.h"
#include "openfeature/general_flag_evaluation_details.h"
#include "openfeature/general_hook.h"
#include "openfeature/general_hook_context.h"
#include "openfeature/hook_hints.h"

namespace codereadiness {

absl::StatusOr<std::shared_ptr<CodeReadinessHook>> CodeReadinessHook::Create(
    std::string current_version, CodeReadinessHookOptions options) {
  if (current_version.empty()) {
    return absl::InvalidArgumentError(
        "codereadiness: current_version cannot be empty");
  }
  std::unique_ptr<VersionComparator> comparator = std::move(options.comparator);
  if (comparator == nullptr) {
    comparator = std::make_unique<SemVerComparator>();
  }
  if (options.min_version_metadata_key.empty()) {
    return absl::InvalidArgumentError(
        "codereadiness: min_version_metadata_key cannot be empty");
  }
  if (absl::Status status = comparator->Initialize(current_version);
      !status.ok()) {
    if (options.strict_validation) {
      return absl::Status(
          status.code(),
          absl::StrCat("codereadiness: comparator initialization failed: ",
                       status.message()));
    }
    VLOG(1) << "codereadiness: invalid current version \"" << current_version
            << "\": " << status;
  }
  return std::shared_ptr<CodeReadinessHook>(new CodeReadinessHook(
      std::move(current_version), std::move(comparator),
      options.strict_validation, std::move(options.min_version_metadata_key)));
}

CodeReadinessHook::CodeReadinessHook(
    std::string current_version, std::unique_ptr<VersionComparator> comparator,
    bool strict_validation, std::string metadata_min_ver_key)
    : current_version_(std::move(current_version)),
      comparator_(std::move(comparator)),
      strict_validation_(strict_validation),
      metadata_min_ver_key_(std::move(metadata_min_ver_key)) {}

void CodeReadinessHook::After(
    const openfeature::GeneralHookContext& ctx,
    const openfeature::GeneralFlagEvaluationDetails& details,
    const openfeature::HookHints& hints) {
  const std::string_view flag_key = ctx.GetFlagKey();
  const auto& metadata = details.GetFlagMetadata().data;
  if (metadata.empty()) {
    FailOrSkip(
        absl::StrCat("Flag metadata is empty for flag \"", flag_key, "\""));
    return;
  }

  auto it_min_code_version = metadata.find(metadata_min_ver_key_);
  if (it_min_code_version == metadata.end()) {
    FailOrSkip(absl::StrCat("key \"", metadata_min_ver_key_,
                            "\" missing in flag's \"", flag_key,
                            "\" metadata"));
    return;
  }

  const std::string* min_code_version =
      std::get_if<std::string>(&it_min_code_version->second);
  if (min_code_version == nullptr) {
    FailOrSkip(absl::StrCat("metadata \"", metadata_min_ver_key_,
                            "\" is not a string for flag \"", flag_key, "\""));
    return;
  }
  if (min_code_version->empty()) {
    FailOrSkip(absl::StrCat("metadata \"", metadata_min_ver_key_,
                            "\" value is empty for flag \"", flag_key, "\""));
    return;
  }

  absl::StatusOr<bool> meets_min_version =
      comparator_->Compare(*min_code_version);
  if (!meets_min_version.ok()) {
    FailOrSkip(absl::StrCat("invalid version values current: \"",
                            current_version_, "\" - minimum: \"",
                            *min_code_version, "\" for flag \"", flag_key,
                            "\": ", meets_min_version.status().message()));
    return;
  }
  if (!*meets_min_version) {
    throw std::runtime_error(
        absl::StrCat("current version: \"", current_version_,
                     "\" required minimum version: \"", *min_code_version,
                     "\" flag \"", flag_key, "\", check failed"));
  }
}

void CodeReadinessHook::FailOrSkip(const std::string& message) const {
  if (strict_validation_) {
    throw std::runtime_error(message);
  }
  VLOG(1) << "codereadiness: " << message << ", skipping validation";
}

}  // namespace codereadiness
