#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
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
bool IsUnmodelledOption(std::string_view option) {
  return option == "resolver" || option == "cache" || option == "maxCacheSize";
}

std::optional<int> ParseOptionInt(std::string_view option,
                                  std::string_view value) {
  const auto parsed = ParseInt64(value);
  if (!parsed.has_value()) {
    FailStep(absl::StrCat("option '", option, "' is not a valid integer: '",
                          value, "'"));
    return std::nullopt;
  }
  return static_cast<int>(*parsed);
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

std::string_view StatusCodeToString(grpc::StatusCode code) {
  switch (code) {
    case grpc::StatusCode::OK:
      return "OK";
    case grpc::StatusCode::CANCELLED:
      return "CANCELLED";
    case grpc::StatusCode::UNKNOWN:
      return "UNKNOWN";
    case grpc::StatusCode::INVALID_ARGUMENT:
      return "INVALID_ARGUMENT";
    case grpc::StatusCode::DEADLINE_EXCEEDED:
      return "DEADLINE_EXCEEDED";
    case grpc::StatusCode::NOT_FOUND:
      return "NOT_FOUND";
    case grpc::StatusCode::ALREADY_EXISTS:
      return "ALREADY_EXISTS";
    case grpc::StatusCode::PERMISSION_DENIED:
      return "PERMISSION_DENIED";
    case grpc::StatusCode::UNAUTHENTICATED:
      return "UNAUTHENTICATED";
    case grpc::StatusCode::RESOURCE_EXHAUSTED:
      return "RESOURCE_EXHAUSTED";
    case grpc::StatusCode::FAILED_PRECONDITION:
      return "FAILED_PRECONDITION";
    case grpc::StatusCode::ABORTED:
      return "ABORTED";
    case grpc::StatusCode::OUT_OF_RANGE:
      return "OUT_OF_RANGE";
    case grpc::StatusCode::UNIMPLEMENTED:
      return "UNIMPLEMENTED";
    case grpc::StatusCode::INTERNAL:
      return "INTERNAL";
    case grpc::StatusCode::UNAVAILABLE:
      return "UNAVAILABLE";
    case grpc::StatusCode::DATA_LOSS:
      return "DATA_LOSS";
    default:
      return "UNKNOWN_STATUS_CODE";
  }
}

std::string StatusCodesToString(const std::vector<grpc::StatusCode>& codes) {
  std::vector<std::string_view> names;
  names.reserve(codes.size());
  for (const grpc::StatusCode code : codes) {
    names.push_back(StatusCodeToString(code));
  }
  return absl::StrJoin(names, ", ");
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

  std::vector<std::string_view> expected_codes;
  for (std::string_view token :
       absl::StrSplit(expected, ',', absl::SkipWhitespace())) {
    const std::string_view trimmed = absl::StripAsciiWhitespace(token);
    if (!trimmed.empty()) {
      expected_codes.push_back(trimmed);
    }
  }

  // The testbed uses placeholder names ("A, B"). FlagdProviderConfig parses
  // the list into grpc::StatusCode and drops what it cannot recognise, so
  // comparing the canonical status code names verifies both count and identity.
  ExpectEq(StatusCodesToString(actual), absl::StrJoin(expected_codes, ", "),
           "option 'fatalStatusCodes' (FlagdProviderConfig discards tokens "
           "that are not valid gRPC status codes)");
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

  for (const auto& [option, value] : Ctx().scenario.pending_options) {
    if (IsUnmodelledOption(option)) {
      continue;
    }
    if (option == "host") {
      config.SetHost(value);
    } else if (option == "port") {
      if (const auto v = ParseOptionInt(option, value)) {
        config.SetPort(*v);
      }
    } else if (option == "tls") {
      if (const auto parsed = ParseBool(value)) {
        config.SetTls(*parsed);
      } else {
        FailStep("option 'tls' is not a boolean: '" + value + "'");
      }
    } else if (option == "deadlineMs") {
      if (const auto v = ParseOptionInt(option, value)) {
        config.SetDeadlineMs(*v);
      }
    } else if (option == "streamDeadlineMs") {
      if (const auto v = ParseOptionInt(option, value)) {
        config.SetStreamDeadlineMs(*v);
      }
    } else if (option == "retryBackoffMs") {
      if (const auto v = ParseOptionInt(option, value)) {
        config.SetRetryBackoffMs(*v);
      }
    } else if (option == "retryBackoffMaxMs") {
      if (const auto v = ParseOptionInt(option, value)) {
        config.SetRetryBackoffMaxMs(*v);
      }
    } else if (option == "retryGracePeriod") {
      if (const auto v = ParseOptionInt(option, value)) {
        config.SetRetryGracePeriod(*v);
      }
    } else if (option == "keepAliveTime") {
      if (const auto v = ParseOptionInt(option, value)) {
        config.SetKeepAliveTimeMs(*v);
      }
    } else if (option == "offlinePollIntervalMs") {
      if (const auto v = ParseOptionInt(option, value)) {
        config.SetOfflinePollIntervalMs(*v);
      }
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
    }
  }

  Ctx().scenario.config = config;
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
