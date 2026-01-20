#include "evaluator.h"

#include <nlohmann/json.hpp>

#include "sync.h"

namespace flagd {

JsonLogicEvaluator::JsonLogicEvaluator(std::shared_ptr<FlagSync> sync)
    : sync_(std::move(sync)) {}

template <typename T>
std::unique_ptr<openfeature::ResolutionDetails<T>>
JsonLogicEvaluator::ResolveAny(std::string_view flag_key, T default_value,
                               const openfeature::EvaluationContext& ctx) {
  std::shared_ptr<const nlohmann::json> flags = sync_->GetFlags();
  if (!flags) {
    return std::make_unique<openfeature::ResolutionDetails<T>>(
        default_value, openfeature::Reason::kError, "",
        openfeature::FlagMetadata(), openfeature::ErrorCode::kParseError,
        "No flags available");
  }

  auto flag_it = flags->find(flag_key);
  if (flag_it == flags->end()) {
    return std::make_unique<openfeature::ResolutionDetails<T>>(
        default_value, openfeature::Reason::kError, "",
        openfeature::FlagMetadata(), openfeature::ErrorCode::kFlagNotFound,
        "Flag not found");
  }

  const nlohmann::json& flag_config = *flag_it;
  std::string variant;

  if (flag_config.contains("targeting")) {
    /*
     * TODO: Invoke JsonLogic here once implemented.
     *
     * Example variables:
     * const nlohmann::json& targeting_rule = flag_config["targeting"];
     *
     * // You would need a way to convert openfeature::EvaluationContext to
     * // nlohmann::json, e.g.:
     * // nlohmann::json data = evaluation_context_to_json(ctx);
     *
     * // Then apply the logic:
     * // nlohmann::json result = JsonLogic::Apply(targeting_rule, data);
     *
     * // If the result is a variant name (string):
     * // if (result.is_string()) {
     * //   variant = result.get<std::string>();
     * // } else if (result.is_bool()) {
     * //   variant = result.get<std::bool>() ? "true" : "false";
     * // }
     */
  }

  openfeature::Reason reason;
  if (!variant.empty()) {
    reason = openfeature::Reason::kTargetingMatch;
  } else {
    // For weird reason the json schema is not verifying that,
    // so we need to check manually.
    if (!flag_config.contains("defaultVariant")) {
      return std::make_unique<openfeature::ResolutionDetails<T>>(
          default_value, openfeature::Reason::kError, "",
          openfeature::FlagMetadata(), openfeature::ErrorCode::kParseError,
          "No default variant defined in flag definition.");
    }

    variant = flag_config["defaultVariant"];
    reason = openfeature::Reason::kStatic;
  }

  const nlohmann::json& variants = flag_config["variants"];

  if (!variants.contains(variant)) {
    return std::make_unique<openfeature::ResolutionDetails<T>>(
        default_value, openfeature::Reason::kError, variant,
        openfeature::FlagMetadata(), openfeature::ErrorCode::kGeneral,
        "Variant not found in config");
  }

  T value;
  try {
    value = variants.at(variant).get<T>();
  } catch (const nlohmann::json::exception& e) {
    return std::make_unique<openfeature::ResolutionDetails<T>>(
        default_value, openfeature::Reason::kError, variant,
        openfeature::FlagMetadata(), openfeature::ErrorCode::kTypeMismatch,
        e.what());
  }

  return std::make_unique<openfeature::ResolutionDetails<T>>(
      value, reason, variant, openfeature::FlagMetadata(), std::nullopt,
      std::nullopt);
}

std::unique_ptr<openfeature::BoolResolutionDetails>
JsonLogicEvaluator::ResolveBoolean(std::string_view flag_key,
                                   bool default_value,
                                   const openfeature::EvaluationContext& ctx) {
  return ResolveAny(flag_key, default_value, ctx);
}

}  // namespace flagd