#include "providers/flagd/tests/gherkin/steps/step_utils.h"

#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "openfeature/error_code.h"
#include "openfeature/general_flag_evaluation_details.h"
#include "openfeature/reason.h"
#include "openfeature/value.h"
#include "providers/flagd/tests/gherkin/test_state.h"

namespace openfeature::contrib::flagd::test {

std::string g_current_selector;

std::string ReasonToString(openfeature::Reason reason) {
  switch (reason) {
    case openfeature::Reason::kStatic:
      return "STATIC";
    case openfeature::Reason::kDefault:
      return "DEFAULT";
    case openfeature::Reason::kTargetingMatch:
      return "TARGETING_MATCH";
    case openfeature::Reason::kSplit:
      return "SPLIT";
    case openfeature::Reason::kCached:
      return "CACHED";
    case openfeature::Reason::kDisabled:
      return "DISABLED";
    case openfeature::Reason::kUnknown:
      return "UNKNOWN";
    case openfeature::Reason::kStale:
      return "STALE";
    case openfeature::Reason::kError:
      return "ERROR";
  }
  return "UNKNOWN_ENUM_VALUE";
}

std::string ErrorCodeToString(openfeature::ErrorCode error_code) {
  switch (error_code) {
    case openfeature::ErrorCode::kProviderNotReady:
      return "PROVIDER_NOT_READY";
    case openfeature::ErrorCode::kFlagNotFound:
      return "FLAG_NOT_FOUND";
    case openfeature::ErrorCode::kParseError:
      return "PARSE_ERROR";
    case openfeature::ErrorCode::kTypeMismatch:
      return "TYPE_MISMATCH";
    case openfeature::ErrorCode::kTargetingKeyMissing:
      return "TARGETING_KEY_MISSING";
    case openfeature::ErrorCode::kInvalidContext:
      return "INVALID_CONTEXT";
    case openfeature::ErrorCode::kProviderFatal:
      return "PROVIDER_FATAL";
    case openfeature::ErrorCode::kGeneral:
      return "GENERAL";
  }
  return "UNKNOWN_ENUM_VALUE";
}

void RecordEvaluationDetails(
    const openfeature::GeneralFlagEvaluationDetails& details) {
  g_state.last_eval.resolved_value = details.GetValueAsValue();
  g_state.last_eval.reason = details.GetReason();
  g_state.last_eval.variant = details.GetVariant();
  g_state.last_eval.error_code = details.GetErrorCode();
  g_state.last_eval.error_message = details.GetErrorMessage();
  g_state.last_eval.flag_metadata = details.GetFlagMetadata();
}

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
    vec.reserve(json_val.size());
    for (const auto& item : json_val) {
      vec.push_back(JsonToValue(item));
    }
    return {vec};
  }
  return {};
}

nlohmann::json ValueToJson(const openfeature::Value& val) {
  if (val.IsNull()) {
    return nullptr;
  }
  if (val.IsBool()) {
    return val.AsBool().value();
  }
  if (val.IsNumber()) {
    if (val.AsInt().has_value()) {
      return val.AsInt().value();
    }
    return val.AsDouble().value();
  }
  if (val.IsString()) {
    return val.AsString().value();
  }
  if (val.IsStructure()) {
    nlohmann::json obj = nlohmann::json::object();
    const auto* map = val.AsStructure();
    for (const auto& [key, value] : *map) {
      obj[key] = ValueToJson(value);
    }
    return obj;
  }
  if (val.IsList()) {
    nlohmann::json arr = nlohmann::json::array();
    const auto* vec = val.AsList();
    for (const auto& item : *vec) {
      arr.push_back(ValueToJson(item));
    }
    return arr;
  }
  return nullptr;
}

}  // namespace openfeature::contrib::flagd::test
