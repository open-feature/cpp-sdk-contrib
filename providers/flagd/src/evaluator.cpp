#include "evaluator.h"

#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "sync.h"

namespace flagd {

namespace {

nlohmann::json ContextToJson(const openfeature::EvaluationContext& ctx) {
  nlohmann::json json = nlohmann::json::object();
  std::optional<std::string_view> targeting_key = ctx.GetTargetingKey();
  if (targeting_key.has_value()) {
    json["targetingKey"] = targeting_key.value();
  }

  for (const auto& [key, value] : ctx.GetAttributes()) {
    if (const auto* str_val = std::any_cast<std::string>(&value)) {
      json[key] = *str_val;
    } else if (const auto* i64_val = std::any_cast<int64_t>(&value)) {
      json[key] = *i64_val;
    } else if (const auto* u64_val = std::any_cast<uint64_t>(&value)) {
      json[key] = *u64_val;
    } else if (const auto* int_val = std::any_cast<int>(&value)) {
      json[key] = *int_val;
    } else if (const auto* dbl_val = std::any_cast<double>(&value)) {
      json[key] = *dbl_val;
    } else if (const auto* bool_val = std::any_cast<bool>(&value)) {
      json[key] = *bool_val;
    } else {
      LOG(WARNING) << "Unsupported attribute type for key: " << key;
    }
  }
  return json;
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
        default_value, openfeature::Reason::kError, "",
        openfeature::FlagMetadata(), openfeature::ErrorCode::kParseError,
        "No flags available");
  }

  auto flag_it = flags->find(flag_key);
  if (flag_it == flags->end()) {
    return std::make_unique<openfeature::ResolutionDetails<T>>(
        default_value, openfeature::Reason::kError, "",
        openfeature::FlagMetadata(), openfeature::ErrorCode::kFlagNotFound,
        absl::StrCat("flag: ", flag_key, " not found"));
  }

  const nlohmann::json& flag_config = *flag_it;

  if (flag_config["state"] == "DISABLED") {
    return std::make_unique<openfeature::ResolutionDetails<T>>(
        default_value, openfeature::Reason::kDisabled, "",
        openfeature::FlagMetadata(), openfeature::ErrorCode::kFlagNotFound,
        absl::StrCat("flag: ", flag_key, " is disabled"));
  }

  std::optional<std::string> variant;

  if (flag_config.contains("targeting") && !flag_config["targeting"].empty()) {
    nlohmann::json data = ContextToJson(ctx);
    // flagd also injects some special variables
    data["$flagd"] = {
        {"flagKey", flag_key},
        {
            "timestamp",
            std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now()),
        },
    };

    absl::StatusOr<nlohmann::json> result =
        json_logic_.Apply(flag_config["targeting"], data);
    if (result.ok()) {
      if (result.value().is_string()) {
        variant = result.value().get<std::string>();
      } else if (result.value().is_boolean()) {
        variant = result.value().get<bool>() ? "true" : "false";
      } else {
        // According to the flagd spec, targeting rules must return a string (the variant name).
        // Booleans are a special case to support logical operations in boolean flags, which
        // we map to "true"/"false" strings. Other types are not valid variant identifiers.
        LOG(WARNING) << "Targeting rule for flag '" << flag_key
                     << "' returned an unsupported type: " << result.value().type_name()
                     << ". Expected string or boolean.";
      }
    } else {
      LOG(ERROR) << "JsonLogic evaluation failed for flag " << flag_key << ": "
                 << result.status().message();
    }
  }

  openfeature::Reason reason;
  std::string variant_name;
  if (variant.has_value()) {
    variant_name = std::move(*variant);
    reason = openfeature::Reason::kTargetingMatch;
  } else {
    if (!flag_config.contains("defaultVariant")) {
      return std::make_unique<openfeature::ResolutionDetails<T>>(
          default_value, openfeature::Reason::kError, "",
          openfeature::FlagMetadata(), openfeature::ErrorCode::kFlagNotFound,
          absl::StrCat("flag: ", flag_key,
                       " doesn't have defaultVariant defined."));
    }

    variant_name = flag_config["defaultVariant"];
    reason = openfeature::Reason::kStatic;
  }

  const nlohmann::json& variants = flag_config["variants"];

  if (!variants.contains(variant_name)) {
    return std::make_unique<openfeature::ResolutionDetails<T>>(
        default_value, openfeature::Reason::kError, variant_name,
        openfeature::FlagMetadata(), openfeature::ErrorCode::kGeneral,
        absl::StrCat("flag: ", flag_key, " doesn't contain evaluated variant: ",
                     variant_name, "."));
  }

  // TODO(#29): Currently this function doesn't differentiate between int and
  // float, We might need to verify those castings once #flagd/1481 issue is
  // resolved.
  T value;
  try {
    value = variants.at(variant_name).get<T>();
  } catch (const nlohmann::json::exception& e) {
    return std::make_unique<openfeature::ResolutionDetails<T>>(
        default_value, openfeature::Reason::kError, variant_name,
        openfeature::FlagMetadata(), openfeature::ErrorCode::kTypeMismatch,
        e.what());
  }

  return std::make_unique<openfeature::ResolutionDetails<T>>(
      value, reason, variant_name, openfeature::FlagMetadata(), std::nullopt,
      std::nullopt);
}

std::unique_ptr<openfeature::BoolResolutionDetails>
JsonLogicEvaluator::ResolveBoolean(std::string_view flag_key,
                                   bool default_value,
                                   const openfeature::EvaluationContext& ctx) {
  return ResolveAny(flag_key, default_value, ctx);
}

}  // namespace flagd