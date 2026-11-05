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
    return absl::Status(
        status.code(),
        absl::StrCat("codereadiness: comparator initialization failed: ",
                     status.message()));
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
    FailOrSkip(flag_key, "flag metadata is empty");
    return;
  }

  auto it_min_code_version = metadata.find(metadata_min_ver_key_);
  if (it_min_code_version == metadata.end()) {
    FailOrSkip(flag_key, absl::StrCat("metadata key \"", metadata_min_ver_key_,
                                      "\" is missing"));
    return;
  }

  const std::string* min_code_version =
      std::get_if<std::string>(&it_min_code_version->second);
  if (min_code_version == nullptr) {
    FailOrSkip(flag_key, absl::StrCat("metadata key \"", metadata_min_ver_key_,
                                      "\" is not a string"));
    return;
  }
  if (min_code_version->empty()) {
    FailOrSkip(flag_key, absl::StrCat("metadata key \"", metadata_min_ver_key_,
                                      "\" value is empty"));
    return;
  }

  absl::StatusOr<bool> meets_min_version =
      comparator_->Compare(*min_code_version);
  if (!meets_min_version.ok()) {
    FailOrSkip(flag_key, absl::StrCat("invalid version values current: \"",
                                      current_version_, "\" - minimum: \"",
                                      *min_code_version, "\": ",
                                      meets_min_version.status().message()));
    return;
  }
  if (!*meets_min_version) {
    throw std::runtime_error(FormatError(
        flag_key, absl::StrCat("current version \"", current_version_,
                               "\" is lower than required minimum version \"",
                               *min_code_version, "\"")));
  }
}

std::string CodeReadinessHook::FormatError(std::string_view flag_key,
                                           std::string_view detail) {
  return absl::StrCat("codereadiness: flag \"", flag_key, "\": ", detail);
}

void CodeReadinessHook::FailOrSkip(std::string_view flag_key,
                                   std::string_view detail) const {
  std::string message = FormatError(flag_key, detail);
  if (strict_validation_) {
    throw std::runtime_error(message);
  }
  VLOG(1) << message << ", skipping validation";
}

}  // namespace codereadiness
