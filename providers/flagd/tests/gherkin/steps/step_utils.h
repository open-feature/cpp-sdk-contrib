#pragma once

#include <cstdint>
#include <format>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

#include "asserts.hpp"  // for cuke::equal
#include "openfeature/error_code.h"
#include "openfeature/general_flag_evaluation_details.h"
#include "openfeature/reason.h"
#include "openfeature/value.h"

// cwt-cucumber expands an empty Examples cell to `""`, which combines with the
// quotes already in the step text to reach the matcher as *four* quotes. The
// built-in `{string}` (`"([^"]*)"`, anchored) cannot match that, so steps whose
// value may be empty are registered twice: once with `{string}` and once with
// this literal. A custom parameter would be tidier, but CUSTOM_PARAMETER and
// step registration race in static init across TUs and throw before main().
#define GHERKIN_EMPTY_ARG "\"\"\"\""

namespace openfeature::contrib::flagd::test {

enum class FlagType { kBoolean, kString, kInteger, kFloat, kObject };

// Terminal `else` of every dispatch chain: a step that recognises none of its
// inputs must fail rather than silently run zero assertions.
void FailStep(const std::string& reason);

void FailStepNotImplemented(const std::string& what);

// cuke::equal only formats "Value X is not equal to Y" when no custom message
// is supplied, so a bare description would throw away the values needed to
// triage the failure.
template <typename T, typename U>
void ExpectEq(const T& actual, const U& expected, std::string_view context) {
  cuke::equal(
      actual, expected,
      std::format("{}: got '{}', expected '{}'", context, actual, expected));
}

std::string ReasonToString(openfeature::Reason reason);
std::string ErrorCodeToString(openfeature::ErrorCode error_code);
std::string FlagTypeToString(FlagType type);
std::optional<FlagType> ParseFlagType(const std::string& name);

void RecordEvaluationDetails(
    const openfeature::GeneralFlagEvaluationDetails& details);

openfeature::Value JsonToValue(const nlohmann::json& json_val);
nlohmann::json ValueToJson(const openfeature::Value& val);

std::optional<int64_t> ParseInt64(const std::string& str);
std::optional<double> ParseDouble(const std::string& str);

std::optional<bool> ParseBool(const std::string& str);

// Guards the static_cast<int64_t>, which is undefined behaviour unless `value`
// is finite, integral and in range.
std::optional<int64_t> AsExactInt64(double value);

// Tolerance scales with magnitude, so large expected values do not fail on
// representation error alone.
bool NearlyEqual(double lhs, double rhs);

}  // namespace openfeature::contrib::flagd::test
