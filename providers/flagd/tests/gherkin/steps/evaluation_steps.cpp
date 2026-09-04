#include <cmath>
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
#include "providers/flagd/tests/gherkin/test_state.h"
#include "table.hpp"

using openfeature::contrib::flagd::test::ErrorCodeToString;
using openfeature::contrib::flagd::test::g_state;
using openfeature::contrib::flagd::test::JsonToValue;
using openfeature::contrib::flagd::test::ReasonToString;
using openfeature::contrib::flagd::test::RecordEvaluationDetails;
using openfeature::contrib::flagd::test::ValueToJson;

WHEN(TheFlagWasEvaluatedWithDetails, "the flag was evaluated with details") {
  ::openfeature::EvaluationContext::Builder builder;
  if (!g_state.targeting_key.empty()) {
    builder.WithTargetingKey(g_state.targeting_key);
  }
  for (const auto& [key, val] : g_state.context_attributes) {
    builder.WithAttribute(key, val);
  }
  for (const auto& [outer_key, inner_map] : g_state.nested_context_attributes) {
    std::map<std::string, ::openfeature::Value> obj_map;
    for (const auto& [inner_key, val] : inner_map) {
      obj_map[inner_key] = val;
    }
    builder.WithAttribute(outer_key, ::openfeature::Value(obj_map));
  }

  ::openfeature::EvaluationContext ctx = builder.build();

  auto& api = ::openfeature::OpenFeatureAPI::GetInstance();
  auto client = api.GetClient();

  std::string type = g_state.last_eval.flag_type;
  std::string key = g_state.last_eval.flag_key;
  std::string def_str = g_state.last_eval.default_value_str;

  if (type == "Boolean") {
    RecordEvaluationDetails(
        client->GetBooleanDetails(key, def_str == "true", ctx));
  } else if (type == "String") {
    RecordEvaluationDetails(client->GetStringDetails(key, def_str, ctx));
  } else if (type == "Integer") {
    int64_t def_val = 0;
    try {
      if (!def_str.empty()) {
        def_val = std::stoll(def_str);
      }
    } catch (...) {
    }
    RecordEvaluationDetails(client->GetIntegerDetails(key, def_val, ctx));
  } else if (type == "Float") {
    double def_val = 0.0;
    try {
      if (!def_str.empty()) {
        def_val = std::stod(def_str);
      }
    } catch (...) {
    }
    RecordEvaluationDetails(client->GetDoubleDetails(key, def_val, ctx));
  } else if (type == "Object") {
    nlohmann::json parsed_json = nlohmann::json::parse(def_str, nullptr, false);
    openfeature::Value def_val = JsonToValue(parsed_json);
    RecordEvaluationDetails(client->GetObjectDetails(key, def_val, ctx));
  }
}

THEN(TheResolvedDetailsValueShouldBe,
     "the resolved details value should be {string}") {
  std::string expected_str = CUKE_ARG(1);
  std::string type = g_state.last_eval.flag_type;

  if (type == "Boolean") {
    bool expected = expected_str == "true";
    auto actual = g_state.last_eval.resolved_value.AsBool();
    cuke::equal(actual.has_value(), true);
    if (actual.has_value()) {
      cuke::equal(actual.value(), expected);
    }
  } else if (type == "String") {
    auto actual = g_state.last_eval.resolved_value.AsString();
    cuke::equal(actual.has_value(), true);
    if (actual.has_value()) {
      cuke::equal(actual.value(), expected_str);
    }
  } else if (type == "Integer") {
    int64_t expected = std::stoll(expected_str);
    auto actual = g_state.last_eval.resolved_value.AsInt();
    cuke::equal(actual.has_value(), true);
    if (actual.has_value()) {
      cuke::equal(actual.value(), expected);
    }
  } else if (type == "Float") {
    double expected = std::stod(expected_str);
    auto actual = g_state.last_eval.resolved_value.AsDouble();
    cuke::equal(actual.has_value(), true);
    if (actual.has_value()) {
      cuke::equal(std::abs(actual.value() - expected) < 1e-5, true);
    }
  } else if (type == "Object") {
    nlohmann::json expected =
        nlohmann::json::parse(expected_str, nullptr, false);
    nlohmann::json actual = ValueToJson(g_state.last_eval.resolved_value);
    cuke::equal(actual.dump(), expected.dump());
  }
}

THEN(TheReasonShouldBe, "the reason should be {string}") {
  std::string expected = CUKE_ARG(1);
  if (expected.empty()) {
    cuke::equal(g_state.last_eval.reason.has_value(), false);
  } else {
    cuke::equal(g_state.last_eval.reason.has_value(), true);
    if (g_state.last_eval.reason.has_value()) {
      std::string actual = ReasonToString(*g_state.last_eval.reason);
      cuke::equal(actual, expected);
    }
  }
}

THEN(TheVariantShouldBe, "the variant should be {string}") {
  std::string expected = CUKE_ARG(1);
  if (expected.empty()) {
    cuke::equal(g_state.last_eval.variant.has_value(), false);
  } else {
    cuke::equal(g_state.last_eval.variant.has_value(), true);
    if (g_state.last_eval.variant.has_value()) {
      cuke::equal(g_state.last_eval.variant.value(), expected);
    }
  }
}

THEN(TheErrorCodeShouldBe, "the error-code should be {string}") {
  std::string expected = CUKE_ARG(1);
  if (expected.empty()) {
    cuke::equal(g_state.last_eval.error_code.has_value(), false);
  } else {
    cuke::equal(g_state.last_eval.error_code.has_value(), true);
    if (g_state.last_eval.error_code.has_value()) {
      cuke::equal(ErrorCodeToString(*g_state.last_eval.error_code), expected);
    }
  }
}

THEN(TheResolvedMetadataIsEmpty, "the resolved metadata is empty") {
  cuke::equal(g_state.last_eval.flag_metadata.data.empty(), true);
}

THEN(TheResolvedMetadataShouldContain, "the resolved metadata should contain") {
  const cuke::table& t = CUKE_TABLE();
  const auto& metadata_map = g_state.last_eval.flag_metadata.data;
  for (const auto& row : t.hashes()) {
    std::string key = row["key"].as<std::string>();
    std::string type = row["metadata_type"].as<std::string>();
    std::string expected_val = row["value"].as<std::string>();

    auto it = metadata_map.find(key);
    cuke::equal(it != metadata_map.end(), true);
    if (it == metadata_map.end()) {
      continue;
    }
    const auto& var_val = it->second;
    if (type == "String") {
      cuke::equal(std::holds_alternative<std::string>(var_val), true);
      if (std::holds_alternative<std::string>(var_val)) {
        cuke::equal(std::get<std::string>(var_val), expected_val);
      }
    } else if (type == "Integer") {
      int64_t expected = std::stoll(expected_val);
      bool is_int = std::holds_alternative<int64_t>(var_val);
      bool is_double = std::holds_alternative<double>(var_val);
      cuke::equal(is_int || is_double, true);
      if (is_int) {
        cuke::equal(std::get<int64_t>(var_val), expected);
      } else if (is_double) {
        cuke::equal(static_cast<int64_t>(std::get<double>(var_val)), expected);
      }
    } else if (type == "Float") {
      double expected = std::stod(expected_val);
      bool is_double = std::holds_alternative<double>(var_val);
      bool is_int = std::holds_alternative<int64_t>(var_val);
      cuke::equal(is_double || is_int, true);
      if (is_double) {
        cuke::equal(std::abs(std::get<double>(var_val) - expected) < 1e-5,
                    true);
      } else if (is_int) {
        cuke::equal(std::abs(static_cast<double>(std::get<int64_t>(var_val)) -
                             expected) < 1e-5,
                    true);
      }
    } else if (type == "Boolean") {
      bool expected = (expected_val == "true" || expected_val == "True");
      cuke::equal(std::holds_alternative<bool>(var_val), true);
      if (std::holds_alternative<bool>(var_val)) {
        cuke::equal(std::get<bool>(var_val), expected);
      }
    }
  }
}
