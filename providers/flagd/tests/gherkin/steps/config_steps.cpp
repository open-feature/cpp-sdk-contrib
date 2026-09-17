#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "asserts.hpp"  // for cuke::equal
#include "defines.hpp"  // for GIVEN, WHEN, THEN
#include "flagd/configuration.h"
#include "get_args.hpp"  // for CUKE_ARG
#include "grpcpp/support/status.h"
#include "providers/flagd/tests/gherkin/steps/step_utils.h"
#include "providers/flagd/tests/gherkin/test_context.h"

namespace {

using openfeature::contrib::flagd::test::Ctx;
using openfeature::contrib::flagd::test::ExpectEq;
using openfeature::contrib::flagd::test::FailStep;
using openfeature::contrib::flagd::test::FailStepNotImplemented;
using openfeature::contrib::flagd::test::ParseBool;
using openfeature::contrib::flagd::test::ParseInt64;

// The provider is in-process only: there is no Resolver type, and
// `cache`/`maxCacheSize` describe the unimplemented RPC resolver's flag cache.
// Asserting on them would mean asserting on a value the test itself invented,
// so they are reported as unimplemented instead.
bool IsUnmodelledOption(const std::string& option) {
  return option == "resolver" || option == "cache" || option == "maxCacheSize";
}

bool ApplyInt(const std::string& option, const std::string& value, int* out) {
  const auto parsed = ParseInt64(value);
  if (!parsed.has_value()) {
    FailStep("option '" + option + "' is not a valid integer: '" + value + "'");
    return false;
  }
  *out = static_cast<int>(*parsed);
  return true;
}

void ExpectOptionalEquals(const std::optional<std::string>& actual,
                          const std::string& expected,
                          const std::string& option) {
  if (expected == "null") {
    cuke::equal(actual.has_value(), false,
                "expected option '" + option + "' to be unset, got '" +
                    actual.value_or("") + "'");
    return;
  }
  if (!actual.has_value()) {
    FailStep("expected option '" + option + "' to be '" + expected +
             "', but it is unset");
    return;
  }
  ExpectEq(*actual, expected, "option '" + option + "'");
}

void ExpectIntEquals(int actual, const std::string& expected,
                     const std::string& option) {
  const auto parsed = ParseInt64(expected);
  if (!parsed.has_value()) {
    FailStep("expected value for option '" + option +
             "' is not a valid integer: '" + expected + "'");
    return;
  }
  ExpectEq(static_cast<int64_t>(actual), *parsed, "option '" + option + "'");
}

std::string StatusCodesToString(const std::vector<grpc::StatusCode>& codes) {
  std::string out;
  for (const grpc::StatusCode code : codes) {
    if (!out.empty()) {
      out += ", ";
    }
    out += std::to_string(static_cast<int>(code));
  }
  return out;
}

void CheckFatalStatusCodes(const ::flagd::FlagdProviderConfig& config,
                           const std::string& expected) {
  const std::vector<grpc::StatusCode>& actual = config.GetFatalStatusCodes();
  if (expected.empty() || expected == "null" || expected == "[]") {
    cuke::equal(actual.empty(), true,
                "expected no fatal status codes, got '" +
                    StatusCodesToString(actual) + "'");
    return;
  }

  // The testbed uses placeholder names ("A, B"). FlagdProviderConfig parses
  // the list into grpc::StatusCode and drops what it cannot recognise, so a
  // placeholder can never round-trip.
  const auto expected_count =
      std::count(expected.begin(), expected.end(), ',') + 1;
  cuke::equal(static_cast<int64_t>(actual.size()),
              static_cast<int64_t>(expected_count),
              "fatalStatusCodes '" + expected + "' produced " +
                  std::to_string(actual.size()) +
                  " parsed code(s); FlagdProviderConfig silently discards "
                  "tokens it cannot map to a grpc::StatusCode");
}

void CheckOptionValue(const std::string& option, const std::string& expected) {
  if (IsUnmodelledOption(option)) {
    FailStepNotImplemented("the '" + option + "' option");
    return;
  }

  const auto& maybe_config = Ctx().scenario.config;
  if (!maybe_config.has_value()) {
    FailStep("no config was initialized before checking option '" + option +
             "'");
    return;
  }
  const ::flagd::FlagdProviderConfig& config = *maybe_config;

  if (option == "host") {
    ExpectEq(config.GetHost(), expected, "option 'host'");
  } else if (option == "port") {
    ExpectIntEquals(config.GetPort(), expected, option);
  } else if (option == "tls") {
    const auto parsed = ParseBool(expected);
    if (!parsed.has_value()) {
      FailStep("expected value for 'tls' is not a boolean: '" + expected + "'");
      return;
    }
    ExpectEq(config.GetTls(), *parsed, "option 'tls'");
  } else if (option == "deadlineMs") {
    ExpectIntEquals(config.GetDeadlineMs(), expected, option);
  } else if (option == "streamDeadlineMs") {
    ExpectIntEquals(config.GetStreamDeadlineMs(), expected, option);
  } else if (option == "retryBackoffMs") {
    ExpectIntEquals(config.GetRetryBackoffMs(), expected, option);
  } else if (option == "retryBackoffMaxMs") {
    ExpectIntEquals(config.GetRetryBackoffMaxMs(), expected, option);
  } else if (option == "retryGracePeriod") {
    ExpectIntEquals(config.GetRetryGracePeriod(), expected, option);
  } else if (option == "keepAliveTime") {
    ExpectIntEquals(config.GetKeepAliveTimeMs(), expected, option);
  } else if (option == "offlinePollIntervalMs") {
    ExpectIntEquals(config.GetOfflinePollIntervalMs(), expected, option);
  } else if (option == "targetUri") {
    ExpectOptionalEquals(config.GetTargetUri(), expected, option);
  } else if (option == "certPath") {
    ExpectOptionalEquals(config.GetCertPath(), expected, option);
  } else if (option == "socketPath") {
    ExpectOptionalEquals(config.GetSocketPath(), expected, option);
  } else if (option == "selector") {
    ExpectOptionalEquals(config.GetSelector(), expected, option);
  } else if (option == "providerId") {
    ExpectOptionalEquals(config.GetProviderId(), expected, option);
  } else if (option == "offlineFlagSourcePath") {
    ExpectOptionalEquals(config.GetOfflineFlagSourcePath(), expected, option);
  } else if (option == "fatalStatusCodes") {
    CheckFatalStatusCodes(config, expected);
  } else {
    FailStep("unknown config option '" + option + "'");
  }
}

}  // namespace

GIVEN(AnEnvironmentVariableWithValue,
      "an environment variable {string} with value {string}") {
  const std::string name = CUKE_ARG(1);
  const std::string value = CUKE_ARG(2);
  Ctx().scenario.env.Set(name, value);
}

WHEN(AConfigWasInitialized, "a config was initialized") {
  // The constructor reads every FLAGD_* environment variable itself, so
  // env-driven scenarios are covered just by constructing it here.
  ::flagd::FlagdProviderConfig config;
  bool ok = true;

  for (const auto& [option, value] : Ctx().scenario.pending_options) {
    if (IsUnmodelledOption(option)) {
      continue;
    }
    int int_value = 0;
    if (option == "host") {
      config.SetHost(value);
    } else if (option == "port") {
      if (ApplyInt(option, value, &int_value))
        config.SetPort(int_value);
      else
        ok = false;
    } else if (option == "tls") {
      const auto parsed = ParseBool(value);
      if (parsed.has_value()) {
        config.SetTls(*parsed);
      } else {
        FailStep("option 'tls' is not a boolean: '" + value + "'");
        ok = false;
      }
    } else if (option == "deadlineMs") {
      if (ApplyInt(option, value, &int_value))
        config.SetDeadlineMs(int_value);
      else
        ok = false;
    } else if (option == "streamDeadlineMs") {
      if (ApplyInt(option, value, &int_value))
        config.SetStreamDeadlineMs(int_value);
      else
        ok = false;
    } else if (option == "retryBackoffMs") {
      if (ApplyInt(option, value, &int_value))
        config.SetRetryBackoffMs(int_value);
      else
        ok = false;
    } else if (option == "retryBackoffMaxMs") {
      if (ApplyInt(option, value, &int_value))
        config.SetRetryBackoffMaxMs(int_value);
      else
        ok = false;
    } else if (option == "retryGracePeriod") {
      if (ApplyInt(option, value, &int_value))
        config.SetRetryGracePeriod(int_value);
      else
        ok = false;
    } else if (option == "keepAliveTime") {
      if (ApplyInt(option, value, &int_value))
        config.SetKeepAliveTimeMs(int_value);
      else
        ok = false;
    } else if (option == "offlinePollIntervalMs") {
      if (ApplyInt(option, value, &int_value))
        config.SetOfflinePollIntervalMs(int_value);
      else
        ok = false;
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
    } else if (option == "fatalStatusCodes") {
      config.SetFatalStatusCodes(value);
    } else {
      FailStep("unknown config option '" + option + "'");
      ok = false;
    }
  }

  Ctx().scenario.config = config;
  // FlagdProviderConfig has no validation entry point, so the only errors the
  // test can observe are the ones it produced applying the options above.
  Ctx().scenario.config_error = !ok;
}

THEN(TheOptionOfTypeShouldHaveValue,
     "the option {string} of type {string} should have the value {string}") {
  CheckOptionValue(CUKE_ARG(1), CUKE_ARG(3));
}

THEN(TheOptionOfTypeShouldHaveEmptyValue,
     "the option {string} of type {string} should have the "
     "value " GHERKIN_EMPTY_ARG) {
  CheckOptionValue(CUKE_ARG(1), "");
}

THEN(WeShouldHaveAnError, "we should have an error") {
  // config.feature reaches this only for the "file" resolver without an
  // offlineFlagSourcePath. FlagdProviderConfig neither models the resolver nor
  // validates that combination, so there is nothing real to assert.
  FailStepNotImplemented("configuration validation");
}
