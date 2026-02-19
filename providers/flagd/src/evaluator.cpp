#include "evaluator.h"

#include <memory>
#include <nlohmann/json.hpp>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "sync.h"

namespace flagd {

namespace {

openfeature::Value JsonToValue(const nlohmann::json& json_val) {
  if (json_val.is_boolean()) {
    return {json_val.get<bool>()};
  }
  if (json_val.is_number_integer()) {
    return {json_val.get<int64_t>()};
  }
  if (json_val.is_number_float()) {
    return {json_val.get<double>()};
  }
  if (json_val.is_string()) {
    return {json_val.get<std::string>()};
  }
  if (json_val.is_object()) {
    std::map<std::string, openfeature::Value> map;
    for (const auto& [key, value] : json_val.items()) {
      map.emplace(key, JsonToValue(value));
    }
    return {map};
  }
  if (json_val.is_array()) {
    std::vector<openfeature::Value> vec;
    for (const auto& item : json_val) {
      vec.push_back(JsonToValue(item));
    }
    return {vec};
  }
  LOG(ERROR) << "Failed to map JSON value to openfeature::Value: "
             << json_val.dump();
  return {};
}

}  // namespace

JsonLogicEvaluator::JsonLogicEvaluator(std::shared_ptr<FlagSync> sync)
    : sync_(std::move(sync)) {}

template <typename T>
std::unique_ptr<openfeature::ResolutionDetails<T>>
JsonLogicEvaluator::ResolveAny(std::string_view flag_key, T default_value,
                               const openfeature::EvaluationContext& ctx) {
  std::shared_ptr<const nlohmann::json> flags = sync_->GetFlags();
  if (flags == nullptr) {
    return std::make_unique<openfeature::ResolutionDetails<T>>(
        std::move(default_value), openfeature::Reason::kError, "",
        openfeature::FlagMetadata(), openfeature::ErrorCode::kParseError,
        "No flags available");
  }

  auto flag_it = flags->find(flag_key);
  if (flag_it == flags->end()) {
    return std::make_unique<openfeature::ResolutionDetails<T>>(
        std::move(default_value), openfeature::Reason::kError, "",
        openfeature::FlagMetadata(), openfeature::ErrorCode::kFlagNotFound,
        absl::StrCat("flag: ", flag_key, " not found"));
  }

  const nlohmann::json& flag_config = *flag_it;

  if (flag_config["state"] == "DISABLED") {
    return std::make_unique<openfeature::ResolutionDetails<T>>(
        std::move(default_value), openfeature::Reason::kDisabled, "",
        openfeature::FlagMetadata(), openfeature::ErrorCode::kFlagNotFound,
        absl::StrCat("flag: ", flag_key, " is disabled"));
  }

  std::string variant;

  if (flag_config.contains("targeting") && !flag_config["targeting"].empty()) {
    /*
     * TODO(#18): Invoke JsonLogic here once implemented.
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
    if (!flag_config.contains("defaultVariant")) {
      return std::make_unique<openfeature::ResolutionDetails<T>>(
          std::move(default_value), openfeature::Reason::kError, "",
          openfeature::FlagMetadata(), openfeature::ErrorCode::kFlagNotFound,
          absl::StrCat("flag: ", flag_key,
                       " doesn't have defaultVariant defined."));
    }

    variant = flag_config["defaultVariant"];
    reason = openfeature::Reason::kStatic;
  }

  const nlohmann::json& variants = flag_config["variants"];

  if (!variants.contains(variant)) {
    return std::make_unique<openfeature::ResolutionDetails<T>>(
        std::move(default_value), openfeature::Reason::kError, variant,
        openfeature::FlagMetadata(), openfeature::ErrorCode::kGeneral,
        absl::StrCat("flag: ", flag_key,
                     " doesn't contain evaluated variant: ", variant, "."));
  }

  // TODO(#29): Currently this function doesn't differentiate between int and
  // float, We might need to verify those castings once #flagd/1481 issue is
  // resolved.
  T value;
  try {
    if constexpr (std::is_same_v<T, openfeature::Value>) {
      value = JsonToValue(variants.at(variant));
    } else {
      value = variants.at(variant).get<T>();
    }
  } catch (const nlohmann::json::exception& err) {
    return std::make_unique<openfeature::ResolutionDetails<T>>(
        std::move(default_value), openfeature::Reason::kError, variant,
        openfeature::FlagMetadata(), openfeature::ErrorCode::kTypeMismatch,
        err.what());
  }

  return std::make_unique<openfeature::ResolutionDetails<T>>(
      std::move(value), reason, variant, openfeature::FlagMetadata(),
      std::nullopt, std::nullopt);
}

std::unique_ptr<openfeature::BoolResolutionDetails>
JsonLogicEvaluator::ResolveBoolean(std::string_view flag_key,
                                   bool default_value,
                                   const openfeature::EvaluationContext& ctx) {
  return ResolveAny(flag_key, default_value, ctx);
}

std::unique_ptr<openfeature::StringResolutionDetails>
JsonLogicEvaluator::ResolveString(std::string_view flag_key,
                                  std::string_view default_value,
                                  const openfeature::EvaluationContext& ctx) {
  return ResolveAny(flag_key, std::string(default_value), ctx);
}

std::unique_ptr<openfeature::IntResolutionDetails>
JsonLogicEvaluator::ResolveInteger(std::string_view flag_key,
                                   int64_t default_value,
                                   const openfeature::EvaluationContext& ctx) {
  return ResolveAny(flag_key, default_value, ctx);
}

std::unique_ptr<openfeature::DoubleResolutionDetails>
JsonLogicEvaluator::ResolveDouble(std::string_view flag_key,
                                  double default_value,
                                  const openfeature::EvaluationContext& ctx) {
  return ResolveAny(flag_key, default_value, ctx);
}

std::unique_ptr<openfeature::ObjectResolutionDetails>
JsonLogicEvaluator::ResolveObject(std::string_view flag_key,
                                  openfeature::Value default_value,
                                  const openfeature::EvaluationContext& ctx) {
  return ResolveAny(flag_key, std::move(default_value), ctx);
}

}  // namespace flagd