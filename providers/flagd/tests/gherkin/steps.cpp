#include <stdlib.h>  // NOLINT(modernize-deprecated-headers)

#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <thread>

// cwt-cucumber internal headers are included directly to satisfy clang-tidy
// misc-include-cleaner, as the library's umbrella header <cucumber.hpp>
// does not explicitly export them.
#include "asserts.hpp"  // for cuke::equal
#include "defines.hpp"  // for GIVEN, WHEN, THEN, BEFORE, AFTER
#include "flagd/configuration.h"
#include "flagd/provider.h"
#include "get_args.hpp"  // for CUKE_ARG, CUKE_TABLE
#include "openfeature/error_code.h"
#include "openfeature/evaluation_context.h"
#include "openfeature/flag_metadata.h"
#include "openfeature/general_flag_evaluation_details.h"
#include "openfeature/openfeature_api.h"
#include "openfeature/reason.h"
#include "openfeature/value.h"
#include "providers/flagd/tests/gherkin/test_env.h"
#include "providers/flagd/tests/gherkin/test_state.h"
#include "table.hpp"

using openfeature::contrib::flagd::test::g_stable_provider;
using openfeature::contrib::flagd::test::g_state;
using openfeature::contrib::flagd::test::ResetTestState;
using openfeature::contrib::flagd::test::SetupGlobalFlagd;

using nlohmann::json;

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

BEFORE(SetupFlagd) {
  ResetTestState();
  SetupGlobalFlagd();
}

AFTER(CleanupFlagd) {
  // Do not stop global flagd between scenarios
}

GIVEN(AnOptionOfTypeWithValue,
      "an option {string} of type {string} with value {string}") {
  std::string option = CUKE_ARG(1);
  std::string type = CUKE_ARG(2);
  std::string value = CUKE_ARG(3);
  g_state.pending_options[option] = value;
  if (option == "cache") {
    g_state.cache_type = value;
  } else if (option == "selector") {
    g_state.selector = value;
  }
}

GIVEN(AStableFlagdProvider, "a stable flagd provider") {
  if (g_stable_provider && g_state.selector == g_current_selector) {
    g_state.provider = g_stable_provider;
    return;
  }

  ::flagd::FlagdProviderConfig config;
  config.SetHost("localhost");
  config.SetPort(8015);
  config.SetDeadlineMs(5000);
  if (!g_state.selector.empty()) {
    config.SetSelector(g_state.selector);
  }

  g_stable_provider = std::make_shared<::flagd::FlagdProvider>(config);
  g_state.provider = g_stable_provider;
  g_current_selector = g_state.selector;

  auto& api = ::openfeature::OpenFeatureAPI::GetInstance();
  api.SetProviderAndWait(g_state.provider);
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

GIVEN(AMetadataFlagdProvider, "a metadata flagd provider") {
  if (g_stable_provider && g_state.selector == g_current_selector) {
    g_state.provider = g_stable_provider;
    return;
  }

  ::flagd::FlagdProviderConfig config;
  config.SetHost("localhost");
  config.SetPort(8015);
  config.SetDeadlineMs(5000);
  if (!g_state.selector.empty()) {
    config.SetSelector(g_state.selector);
  }

  g_stable_provider = std::make_shared<::flagd::FlagdProvider>(config);
  g_state.provider = g_stable_provider;
  g_current_selector = g_state.selector;

  auto& api = ::openfeature::OpenFeatureAPI::GetInstance();
  api.SetProviderAndWait(g_state.provider);
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

GIVEN(AnEvaluator, "an evaluator") {
  if (g_stable_provider && g_state.selector == g_current_selector) {
    g_state.provider = g_stable_provider;
    return;
  }

  ::flagd::FlagdProviderConfig config;
  config.SetHost("localhost");
  config.SetPort(8015);
  config.SetDeadlineMs(5000);
  if (!g_state.selector.empty()) {
    config.SetSelector(g_state.selector);
  }

  g_stable_provider = std::make_shared<::flagd::FlagdProvider>(config);
  g_state.provider = g_stable_provider;
  g_current_selector = g_state.selector;

  auto& api = ::openfeature::OpenFeatureAPI::GetInstance();
  api.SetProviderAndWait(g_state.provider);
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

GIVEN(ABooleanFlag,
      "a Boolean-flag with key {string} and a default value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Boolean";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AStringFlag,
      "a String-flag with key {string} and a default value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "String";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AIntegerFlag,
      "a Integer-flag with key {string} and a default value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Integer";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AFloatFlag,
      "a Float-flag with key {string} and a default value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Float";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AnObjectFlag,
      "a Object-flag with key {string} and a default value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Object";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(ABooleanFlagFallback,
      "a Boolean-flag with key {string} and a fallback value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Boolean";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AStringFlagFallback,
      "a String-flag with key {string} and a fallback value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "String";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AIntegerFlagFallback,
      "a Integer-flag with key {string} and a fallback value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Integer";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AFloatFlagFallback,
      "a Float-flag with key {string} and a fallback value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Float";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AnObjectFlagFallback,
      "a Object-flag with key {string} and a fallback value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Object";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AContextContainingKeyTypeValue,
      "a context containing a key {string}, with type {string} and with value "
      "{string}") {
  std::string key = CUKE_ARG(1);
  std::string type = CUKE_ARG(2);
  std::string value = CUKE_ARG(3);

  if (key == "targetingKey") {
    g_state.targeting_key = value;
  } else {
    if (type == "String") {
      g_state.context_attributes[key] = value;
    } else if (type == "Boolean") {
      g_state.context_attributes[key] = value == "true";
    } else if (type == "Integer") {
      g_state.context_attributes[key] = static_cast<int64_t>(std::stoll(value));
    } else if (type == "Float") {
      g_state.context_attributes[key] = std::stod(value);
    }
  }
}

GIVEN(AContextContainingTargetingKey,
      "a context containing a targeting key with value {string}") {
  g_state.targeting_key = static_cast<std::string>(CUKE_ARG(1));
}

GIVEN(AContextContainingNestedProperty,
      "a context containing a nested property with outer key {string} and "
      "inner key {string}, with value {string}") {
  std::string outer_key = CUKE_ARG(1);
  std::string inner_key = CUKE_ARG(2);
  std::string value = CUKE_ARG(3);

  g_state.nested_context_attributes[outer_key][inner_key] =
      ::openfeature::Value(value);
}

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

GIVEN(AnEnvironmentVariableWithValue,
      "an environment variable {string} with value {string}") {
  std::string env_var = CUKE_ARG(1);
  std::string value = CUKE_ARG(2);
  setenv(env_var.c_str(), value.c_str(), 1);
  g_state.set_env_vars.push_back(env_var);
}

WHEN(AConfigWasInitialized, "a config was initialized") {
  try {
    ::flagd::FlagdProviderConfig config;
    for (const auto& [option, value] : g_state.pending_options) {
      if (option == "host") {
        config.SetHost(value);
      } else if (option == "port") {
        config.SetPort(std::stoi(value));
      } else if (option == "tls") {
        config.SetTls(value == "true" || value == "True");
      } else if (option == "deadlineMs") {
        config.SetDeadlineMs(std::stoi(value));
      } else if (option == "streamDeadlineMs") {
        config.SetStreamDeadlineMs(std::stoi(value));
      } else if (option == "retryBackoffMs") {
        config.SetRetryBackoffMs(std::stoi(value));
      } else if (option == "retryBackoffMaxMs") {
        config.SetRetryBackoffMaxMs(std::stoi(value));
      } else if (option == "retryGracePeriod") {
        config.SetRetryGracePeriod(std::stoi(value));
      } else if (option == "keepAliveTime") {
        config.SetKeepAliveTimeMs(std::stoi(value));
      } else if (option == "targetUri") {
        config.SetTargetUri(value);
      } else if (option == "certPath") {
        config.SetCertPath(value);
      } else if (option == "socketPath") {
        config.SetSocketPath(value);
      } else if (option == "selector") {
        config.SetSelector(value);
      } else if (option == "providerId") {
        config.SetProviderId(value);
      } else if (option == "offlineFlagSourcePath") {
        config.SetOfflineFlagSourcePath(value);
      } else if (option == "offlinePollIntervalMs") {
        config.SetOfflinePollIntervalMs(std::stoi(value));
      } else if (option == "fatalStatusCodes") {
        config.SetFatalStatusCodes(value);
      }
    }
    g_state.config = config;
    g_state.config_error = false;
  } catch (...) {
    g_state.config_error = true;
  }
}

THEN(TheOptionOfTypeShouldHaveValue,
     "the option {string} of type {string} should have the value {string}") {
  std::string option = CUKE_ARG(1);
  std::string type = CUKE_ARG(2);
  std::string expected_val = CUKE_ARG(3);

  cuke::equal(g_state.config.has_value(), true);
  if (!g_state.config.has_value()) {
    return;
  }
  const auto& config = g_state.config.value();

  if (option == "host") {
    cuke::equal(config.GetHost(), expected_val);
  } else if (option == "port") {
    cuke::equal(config.GetPort(), std::stoi(expected_val));
  } else if (option == "tls") {
    bool expected = expected_val == "true" || expected_val == "True";
    cuke::equal(config.GetTls(), expected);
  } else if (option == "deadlineMs") {
    cuke::equal(config.GetDeadlineMs(), std::stoi(expected_val));
  } else if (option == "streamDeadlineMs") {
    cuke::equal(config.GetStreamDeadlineMs(), std::stoi(expected_val));
  } else if (option == "retryBackoffMs") {
    cuke::equal(config.GetRetryBackoffMs(), std::stoi(expected_val));
  } else if (option == "retryBackoffMaxMs") {
    cuke::equal(config.GetRetryBackoffMaxMs(), std::stoi(expected_val));
  } else if (option == "retryGracePeriod") {
    cuke::equal(config.GetRetryGracePeriod(), std::stoi(expected_val));
  } else if (option == "keepAliveTime") {
    cuke::equal(config.GetKeepAliveTimeMs(), std::stoi(expected_val));
  } else if (option == "targetUri") {
    auto val = config.GetTargetUri();
    cuke::equal(val.has_value(), true);
    if (val.has_value()) {
      cuke::equal(val.value(), expected_val);
    }
  } else if (option == "certPath") {
    auto val = config.GetCertPath();
    cuke::equal(val.has_value(), true);
    if (val.has_value()) {
      cuke::equal(val.value(), expected_val);
    }
  } else if (option == "socketPath") {
    auto val = config.GetSocketPath();
    cuke::equal(val.has_value(), true);
    if (val.has_value()) {
      cuke::equal(val.value(), expected_val);
    }
  } else if (option == "selector") {
    auto val = config.GetSelector();
    cuke::equal(val.has_value(), true);
    if (val.has_value()) {
      cuke::equal(val.value(), expected_val);
    }
  } else if (option == "providerId") {
    auto val = config.GetProviderId();
    cuke::equal(val.has_value(), true);
    if (val.has_value()) {
      cuke::equal(val.value(), expected_val);
    }
  } else if (option == "offlineFlagSourcePath") {
    auto val = config.GetOfflineFlagSourcePath();
    cuke::equal(val.has_value(), true);
    if (val.has_value()) {
      cuke::equal(val.value(), expected_val);
    }
  } else if (option == "offlinePollIntervalMs") {
    cuke::equal(config.GetOfflinePollIntervalMs(), std::stoi(expected_val));
  } else if (option == "resolver") {
    if (expected_val == "in-process") {
      // OK
    } else {
      cuke::equal(false, true);
    }
  }
}

THEN(WeShouldHaveAnError, "we should have an error") {
  cuke::equal(g_state.config_error, true);
}

AFTER(CleanupEnv) {
  for (const auto& var : g_state.set_env_vars) {
    unsetenv(var.c_str());
  }
  g_state.set_env_vars.clear();
}
