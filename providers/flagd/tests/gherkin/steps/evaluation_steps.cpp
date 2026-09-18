#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <variant>

#include "asserts.hpp"   // for cuke::equal
#include "defines.hpp"   // for WHEN, THEN
#include "get_args.hpp"  // for CUKE_ARG, CUKE_TABLE
#include "openfeature/evaluation_context.h"
#include "openfeature/openfeature_api.h"
#include "openfeature/value.h"
#include "providers/flagd/tests/gherkin/steps/step_utils.h"
#include "providers/flagd/tests/gherkin/test_context.h"
#include "table.hpp"

namespace {

using openfeature::contrib::flagd::test::AsExactInt64;
using openfeature::contrib::flagd::test::Ctx;
using openfeature::contrib::flagd::test::ErrorCodeToString;
using openfeature::contrib::flagd::test::ExpectEq;
using openfeature::contrib::flagd::test::FailStep;
using openfeature::contrib::flagd::test::FlagType;
using openfeature::contrib::flagd::test::FlagTypeToString;
using openfeature::contrib::flagd::test::JsonToValue;
using openfeature::contrib::flagd::test::NearlyEqual;
using openfeature::contrib::flagd::test::ParseBool;
using openfeature::contrib::flagd::test::ParseDouble;
using openfeature::contrib::flagd::test::ParseInt64;
using openfeature::contrib::flagd::test::ReasonToString;
using openfeature::contrib::flagd::test::RecordEvaluationDetails;
using openfeature::contrib::flagd::test::ValueToJson;

::openfeature::EvaluationContext BuildEvaluationContext() {
  ::openfeature::EvaluationContext::Builder builder;
  const auto& scenario = Ctx().scenario;

  if (!scenario.targeting_key.empty()) {
    builder.WithTargetingKey(scenario.targeting_key);
  }
  for (const auto& [key, value] : scenario.context_attributes) {
    builder.WithAttribute(key, value);
  }
  for (const auto& [outer_key, inner_map] :
       scenario.nested_context_attributes) {
    std::map<std::string, ::openfeature::Value> object;
    for (const auto& [inner_key, value] : inner_map) {
      object[inner_key] = value;
    }
    builder.WithAttribute(outer_key, ::openfeature::Value(object));
  }
  return builder.build();
}

// Without this guard a THEN step reached without a WHEN step would assert
// against a default-constructed result and report success.
const openfeature::contrib::flagd::test::EvaluationResult* RequireEvaluation() {
  const auto& result = Ctx().scenario.last_eval;
  if (!result.recorded) {
    FailStep(
        "no evaluation has been recorded; the 'the flag was evaluated with "
        "details' step did not run or did not resolve the flag");
    return nullptr;
  }
  return &result;
}

void CheckResolvedValue(const std::string& expected_str) {
  const auto* result = RequireEvaluation();
  if (result == nullptr) {
    return;
  }

  switch (Ctx().scenario.pending_eval.flag_type) {
    case FlagType::kBoolean: {
      const auto expected = ParseBool(expected_str);
      if (!expected.has_value()) {
        FailStep("expected Boolean value is not parseable: '" + expected_str +
                 "'");
        return;
      }
      const auto actual = result->value.AsBool();
      if (!actual.has_value()) {
        FailStep("resolved value is not a Boolean");
        return;
      }
      ExpectEq(*actual, *expected, "resolved Boolean mismatch");
      return;
    }
    case FlagType::kString: {
      const auto actual = result->value.AsString();
      if (!actual.has_value()) {
        FailStep("resolved value is not a String");
        return;
      }
      ExpectEq(*actual, expected_str, "resolved String mismatch");
      return;
    }
    case FlagType::kInteger: {
      const auto expected = ParseInt64(expected_str);
      if (!expected.has_value()) {
        FailStep("expected Integer value is not a valid int64: '" +
                 expected_str + "'");
        return;
      }
      const auto actual = result->value.AsInt();
      if (!actual.has_value()) {
        FailStep("resolved value is not an Integer");
        return;
      }
      ExpectEq(*actual, *expected, "resolved Integer mismatch");
      return;
    }
    case FlagType::kFloat: {
      const auto expected = ParseDouble(expected_str);
      if (!expected.has_value()) {
        FailStep("expected Float value is not a valid double: '" +
                 expected_str + "'");
        return;
      }
      const auto actual = result->value.AsDouble();
      if (!actual.has_value()) {
        FailStep("resolved value is not a Float");
        return;
      }
      cuke::equal(NearlyEqual(*actual, *expected), true,
                  "resolved Float mismatch: got " + std::to_string(*actual) +
                      ", expected " + std::to_string(*expected));
      return;
    }
    case FlagType::kObject: {
      const nlohmann::json expected =
          nlohmann::json::parse(expected_str, nullptr, false);
      if (expected.is_discarded()) {
        FailStep("expected JSON is malformed: '" + expected_str + "'");
        return;
      }
      const nlohmann::json actual = ValueToJson(result->value);
      cuke::equal(actual == expected, true,
                  "resolved Object mismatch. Actual: " + actual.dump() +
                      ", expected: " + expected.dump());
      return;
    }
  }
  FailStep("unhandled flag type in resolved-value assertion");
}

void CheckMetadataEntry(
    const std::string& key, const std::string& type,
    const std::string& expected_val,
    const std::unordered_map<std::string, ::openfeature::FlagMetadataValue>&
        metadata) {
  const auto it = metadata.find(key);
  if (it == metadata.end()) {
    FailStep("resolved metadata has no key '" + key + "'");
    return;
  }
  const auto& value = it->second;

  if (type == "String") {
    if (!std::holds_alternative<std::string>(value)) {
      FailStep("metadata '" + key + "' is not a String");
      return;
    }
    ExpectEq(std::get<std::string>(value), expected_val,
             "metadata '" + key + "'");
  } else if (type == "Integer") {
    const auto expected = ParseInt64(expected_val);
    if (!expected.has_value()) {
      FailStep("expected Integer metadata is not a valid int64: '" +
               expected_val + "'");
      return;
    }
    if (std::holds_alternative<int64_t>(value)) {
      ExpectEq(std::get<int64_t>(value), *expected, "metadata '" + key + "'");
      return;
    }
    if (std::holds_alternative<double>(value)) {
      // flagd round-trips metadata through JSON, so a whole number may arrive
      // as a double. Accept it only when it is exactly integral.
      const auto as_int = AsExactInt64(std::get<double>(value));
      if (!as_int.has_value()) {
        FailStep("metadata '" + key +
                 "' is a non-integral double where an Integer was expected");
        return;
      }
      ExpectEq(*as_int, *expected, "metadata '" + key + "'");
      return;
    }
    FailStep("metadata '" + key + "' is neither Integer nor Float");
  } else if (type == "Float") {
    const auto expected = ParseDouble(expected_val);
    if (!expected.has_value()) {
      FailStep("expected Float metadata is not a valid double: '" +
               expected_val + "'");
      return;
    }
    double actual = 0.0;
    if (std::holds_alternative<double>(value)) {
      actual = std::get<double>(value);
    } else if (std::holds_alternative<int64_t>(value)) {
      actual = static_cast<double>(std::get<int64_t>(value));
    } else {
      FailStep("metadata '" + key + "' is neither Float nor Integer");
      return;
    }
    cuke::equal(NearlyEqual(actual, *expected), true,
                "metadata '" + key + "' mismatch: got " +
                    std::to_string(actual) + ", expected " +
                    std::to_string(*expected));
  } else if (type == "Boolean") {
    const auto expected = ParseBool(expected_val);
    if (!expected.has_value()) {
      FailStep("expected Boolean metadata is not parseable: '" + expected_val +
               "'");
      return;
    }
    if (!std::holds_alternative<bool>(value)) {
      FailStep("metadata '" + key + "' is not a Boolean");
      return;
    }
    ExpectEq(std::get<bool>(value), *expected, "metadata '" + key + "'");
  } else {
    FailStep("unsupported metadata_type '" + type + "' for key '" + key + "'");
  }
}

}  // namespace

WHEN(TheFlagWasEvaluatedWithDetails, "the flag was evaluated with details") {
  const auto& pending = Ctx().scenario.pending_eval;
  if (!pending.declared) {
    FailStep("no flag was declared before evaluation");
    return;
  }

  const ::openfeature::EvaluationContext ctx = BuildEvaluationContext();
  auto client = ::openfeature::OpenFeatureAPI::GetInstance().GetClient();
  if (client == nullptr) {
    FailStep("OpenFeatureAPI returned no client; was a provider registered?");
    return;
  }

  const std::string& key = pending.flag_key;
  const std::string& def_str = pending.default_value_str;

  switch (pending.flag_type) {
    case FlagType::kBoolean: {
      const auto def_val = ParseBool(def_str);
      if (!def_val.has_value()) {
        FailStep("default Boolean value is not parseable: '" + def_str + "'");
        return;
      }
      RecordEvaluationDetails(client->GetBooleanDetails(key, *def_val, ctx));
      return;
    }
    case FlagType::kString:
      RecordEvaluationDetails(client->GetStringDetails(key, def_str, ctx));
      return;
    case FlagType::kInteger: {
      const auto def_val = ParseInt64(def_str);
      if (!def_val.has_value()) {
        FailStep("default Integer value is not a valid int64: '" + def_str +
                 "'");
        return;
      }
      RecordEvaluationDetails(client->GetIntegerDetails(key, *def_val, ctx));
      return;
    }
    case FlagType::kFloat: {
      const auto def_val = ParseDouble(def_str);
      if (!def_val.has_value()) {
        FailStep("default Float value is not a valid double: '" + def_str +
                 "'");
        return;
      }
      RecordEvaluationDetails(client->GetDoubleDetails(key, *def_val, ctx));
      return;
    }
    case FlagType::kObject: {
      const nlohmann::json parsed =
          nlohmann::json::parse(def_str, nullptr, false);
      if (parsed.is_discarded()) {
        FailStep("default Object value is not valid JSON: '" + def_str + "'");
        return;
      }
      RecordEvaluationDetails(
          client->GetObjectDetails(key, JsonToValue(parsed), ctx));
      return;
    }
  }
  FailStep("unhandled flag type '" + FlagTypeToString(pending.flag_type) +
           "' during evaluation");
}

THEN(TheResolvedDetailsValueShouldBe,
     "the resolved details value should be {string}") {
  CheckResolvedValue(CUKE_ARG(1));
}

THEN(TheResolvedDetailsValueShouldBeEmpty,
     "the resolved details value should be " GHERKIN_EMPTY_ARG) {
  CheckResolvedValue("");
}

THEN(TheReasonShouldBe, "the reason should be {string}") {
  const auto* result = RequireEvaluation();
  if (result == nullptr) {
    return;
  }
  const std::string expected = CUKE_ARG(1);
  if (!result->reason.has_value()) {
    FailStep("no reason was returned, expected '" + expected + "'");
    return;
  }
  ExpectEq(ReasonToString(*result->reason), expected, "reason mismatch");
}

THEN(TheReasonShouldBeEmpty, "the reason should be " GHERKIN_EMPTY_ARG) {
  const auto* result = RequireEvaluation();
  if (result == nullptr) {
    return;
  }
  cuke::equal(result->reason.has_value(), false,
              "expected no reason, but one was returned");
}

THEN(TheVariantShouldBe, "the variant should be {string}") {
  const auto* result = RequireEvaluation();
  if (result == nullptr) {
    return;
  }
  const std::string expected = CUKE_ARG(1);
  if (!result->variant.has_value()) {
    FailStep("no variant was returned, expected '" + expected + "'");
    return;
  }
  ExpectEq(*result->variant, expected, "variant mismatch");
}

THEN(TheVariantShouldBeEmpty, "the variant should be " GHERKIN_EMPTY_ARG) {
  const auto* result = RequireEvaluation();
  if (result == nullptr) {
    return;
  }
  cuke::equal(
      result->variant.value_or("").empty(), true,
      "expected no variant, got '" + result->variant.value_or("") + "'");
}

THEN(TheErrorCodeShouldBe, "the error-code should be {string}") {
  const auto* result = RequireEvaluation();
  if (result == nullptr) {
    return;
  }
  const std::string expected = CUKE_ARG(1);
  if (!result->error_code.has_value()) {
    FailStep("no error-code was returned, expected '" + expected + "'");
    return;
  }
  ExpectEq(ErrorCodeToString(*result->error_code), expected,
           "error-code mismatch");
}

// A blank error_code column asserts that the evaluation produced *no* error,
// so this twin carries real meaning rather than just covering a parse quirk.
THEN(TheErrorCodeShouldBeEmpty, "the error-code should be " GHERKIN_EMPTY_ARG) {
  const auto* result = RequireEvaluation();
  if (result == nullptr) {
    return;
  }
  cuke::equal(result->error_code.has_value(), false,
              "expected no error-code, got '" +
                  (result->error_code.has_value()
                       ? ErrorCodeToString(*result->error_code)
                       : std::string()) +
                  "'");
}

THEN(TheResolvedMetadataIsEmpty, "the resolved metadata is empty") {
  const auto* result = RequireEvaluation();
  if (result == nullptr) {
    return;
  }
  cuke::equal(result->flag_metadata.data.empty(), true,
              "expected empty resolved metadata");
}

THEN(TheResolvedMetadataShouldContain, "the resolved metadata should contain") {
  const auto* result = RequireEvaluation();
  if (result == nullptr) {
    return;
  }
  const cuke::table& table = CUKE_TABLE();
  for (const auto& row : table.hashes()) {
    CheckMetadataEntry(
        row["key"].as<std::string>(), row["metadata_type"].as<std::string>(),
        row["value"].as<std::string>(), result->flag_metadata.data);
  }
}
