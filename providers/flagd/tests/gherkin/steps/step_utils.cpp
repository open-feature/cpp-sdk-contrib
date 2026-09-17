#include "providers/flagd/tests/gherkin/steps/step_utils.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "asserts.hpp"  // for cuke::equal
#include "openfeature/error_code.h"
#include "openfeature/general_flag_evaluation_details.h"
#include "openfeature/reason.h"
#include "openfeature/value.h"
#include "providers/flagd/tests/gherkin/test_context.h"

namespace openfeature::contrib::flagd::test {

void FailStep(const std::string& reason) { cuke::equal(true, false, reason); }

void FailStepNotImplemented(const std::string& what) {
  FailStep(what +
           " has no representation in FlagdProviderConfig/FlagdProvider yet, "
           "so this expectation cannot be verified");
}

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

std::string FlagTypeToString(FlagType type) {
  switch (type) {
    case FlagType::kBoolean:
      return "Boolean";
    case FlagType::kString:
      return "String";
    case FlagType::kInteger:
      return "Integer";
    case FlagType::kFloat:
      return "Float";
    case FlagType::kObject:
      return "Object";
  }
  return "UNKNOWN_ENUM_VALUE";
}

std::optional<FlagType> ParseFlagType(const std::string& name) {
  if (name == "Boolean") return FlagType::kBoolean;
  if (name == "String") return FlagType::kString;
  if (name == "Integer") return FlagType::kInteger;
  if (name == "Float") return FlagType::kFloat;
  if (name == "Object") return FlagType::kObject;
  return std::nullopt;
}

void RecordEvaluationDetails(
    const openfeature::GeneralFlagEvaluationDetails& details) {
  EvaluationResult& result = Ctx().scenario.last_eval;
  result.recorded = true;
  result.value = details.GetValueAsValue();
  result.reason = details.GetReason();
  result.variant = details.GetVariant();
  result.error_code = details.GetErrorCode();
  result.error_message = details.GetErrorMessage();
  result.flag_metadata = details.GetFlagMetadata();
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
    const double as_double = val.AsDouble().value();
    // Emit whole numbers as JSON integers so they compare equal to the
    // integer literals in the feature files. AsExactInt64 refuses values the
    // int64_t cast could not represent.
    if (std::optional<int64_t> exact = AsExactInt64(as_double)) {
      return *exact;
    }
    return as_double;
  }
  if (val.IsString()) {
    return val.AsString().value();
  }
  if (val.IsStructure()) {
    nlohmann::json obj = nlohmann::json::object();
    for (const auto& [key, value] : *val.AsStructure()) {
      obj[key] = ValueToJson(value);
    }
    return obj;
  }
  if (val.IsList()) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& item : *val.AsList()) {
      arr.push_back(ValueToJson(item));
    }
    return arr;
  }
  return nullptr;
}

std::optional<int64_t> ParseInt64(const std::string& str) {
  try {
    size_t idx = 0;
    int64_t val = std::stoll(str, &idx);
    if (idx != str.size()) {
      return std::nullopt;
    }
    return val;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<double> ParseDouble(const std::string& str) {
  try {
    size_t idx = 0;
    double val = std::stod(str, &idx);
    if (idx != str.size()) {
      return std::nullopt;
    }
    return val;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<bool> ParseBool(const std::string& str) {
  if (str == "true" || str == "True") return true;
  if (str == "false" || str == "False") return false;
  return std::nullopt;
}

std::optional<int64_t> AsExactInt64(double value) {
  if (std::isnan(value) || std::isinf(value)) {
    return std::nullopt;
  }
  // 2^63 is the first double above the int64_t range; the lower bound is
  // exactly representable, the upper bound is not, hence the asymmetry.
  constexpr double kMin = -9223372036854775808.0;
  constexpr double kMax = 9223372036854775808.0;
  if (value < kMin || value >= kMax) {
    return std::nullopt;
  }
  if (std::trunc(value) != value) {
    return std::nullopt;
  }
  return static_cast<int64_t>(value);
}

bool NearlyEqual(double lhs, double rhs) {
  if (lhs == rhs) {
    return true;
  }
  if (std::isnan(lhs) || std::isnan(rhs)) {
    return false;
  }
  constexpr double kRelativeTolerance = 1e-9;
  constexpr double kAbsoluteTolerance = 1e-9;
  const double diff = std::abs(lhs - rhs);
  if (diff <= kAbsoluteTolerance) {
    return true;
  }
  const double scale = std::max(std::abs(lhs), std::abs(rhs));
  if (scale > std::numeric_limits<double>::max() / 2) {
    return false;
  }
  return diff <= kRelativeTolerance * scale;
}

}  // namespace openfeature::contrib::flagd::test
