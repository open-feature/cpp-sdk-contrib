#include <cctype>
#include <cstdlib>
#include <string>

#include "asserts.hpp"  // for cuke::equal
#include "defines.hpp"  // for GIVEN, WHEN, THEN, AFTER
#include "flagd/configuration.h"
#include "get_args.hpp"  // for CUKE_ARG
#include "providers/flagd/tests/gherkin/test_state.h"

using openfeature::contrib::flagd::test::g_state;

namespace {

void CheckOptionValue(const std::string& option, const std::string& type,
                      const std::string& expected_val) {
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
    if (expected_val == "null") {
      cuke::equal(val.has_value(), false);
    } else {
      cuke::equal(val.has_value(), true);
      if (val.has_value()) {
        cuke::equal(val.value(), expected_val);
      }
    }
  } else if (option == "certPath") {
    auto val = config.GetCertPath();
    if (expected_val == "null") {
      cuke::equal(val.has_value(), false);
    } else {
      cuke::equal(val.has_value(), true);
      if (val.has_value()) {
        cuke::equal(val.value(), expected_val);
      }
    }
  } else if (option == "socketPath") {
    auto val = config.GetSocketPath();
    if (expected_val == "null") {
      cuke::equal(val.has_value(), false);
    } else {
      cuke::equal(val.has_value(), true);
      if (val.has_value()) {
        cuke::equal(val.value(), expected_val);
      }
    }
  } else if (option == "selector") {
    auto val = config.GetSelector();
    if (expected_val == "null") {
      cuke::equal(val.has_value(), false);
    } else {
      cuke::equal(val.has_value(), true);
      if (val.has_value()) {
        cuke::equal(val.value(), expected_val);
      }
    }
  } else if (option == "providerId") {
    auto val = config.GetProviderId();
    if (expected_val == "null") {
      cuke::equal(val.has_value(), false);
    } else {
      cuke::equal(val.has_value(), true);
      if (val.has_value()) {
        cuke::equal(val.value(), expected_val);
      }
    }
  } else if (option == "offlineFlagSourcePath") {
    auto val = config.GetOfflineFlagSourcePath();
    if (expected_val == "null") {
      cuke::equal(val.has_value(), false);
    } else {
      cuke::equal(val.has_value(), true);
      if (val.has_value()) {
        cuke::equal(val.value(), expected_val);
      }
    }
  } else if (option == "offlinePollIntervalMs") {
    cuke::equal(config.GetOfflinePollIntervalMs(), std::stoi(expected_val));
  } else if (option == "fatalStatusCodes") {
    if (expected_val.empty() || expected_val == "null" ||
        expected_val == "[]") {
      cuke::equal(g_state.fatal_status_codes_str.empty(), true);
    } else {
      cuke::equal(g_state.fatal_status_codes_str, expected_val);
    }
  } else if (option == "resolver") {
    std::string expected_res = expected_val;
    for (char& c : expected_res) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    cuke::equal(g_state.resolved_resolver, expected_res);
  }
}

}  // namespace

GIVEN(AnEnvironmentVariableWithValue,
      "an environment variable {string} with value {string}") {
  std::string env_var = CUKE_ARG(1);
  std::string value = CUKE_ARG(2);
  if (!g_state.saved_env_vars.contains(env_var)) {
    const char* cur = std::getenv(env_var.c_str());
    if (cur != nullptr) {
      g_state.saved_env_vars[env_var] = std::string(cur);
    } else {
      g_state.saved_env_vars[env_var] = std::nullopt;
    }
  }
  setenv(env_var.c_str(), value.c_str(), 1);
}

WHEN(AConfigWasInitialized, "a config was initialized") {
  try {
    ::flagd::FlagdProviderConfig config;

    std::string resolver = "rpc";
    if (const char* env_res = std::getenv("FLAGD_RESOLVER")) {
      resolver = env_res;
    }
    auto opt_it = g_state.pending_options.find("resolver");
    if (opt_it != g_state.pending_options.end()) {
      resolver = opt_it->second;
    }
    for (char& c : resolver) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    bool has_offline_path = config.GetOfflineFlagSourcePath().has_value();
    auto path_it = g_state.pending_options.find("offlineFlagSourcePath");
    if (path_it != g_state.pending_options.end()) {
      has_offline_path = !path_it->second.empty();
    } else if (const char* env_path =
                   std::getenv("FLAGD_OFFLINE_FLAG_SOURCE_PATH")) {
      has_offline_path = *env_path != '\0';
    }

    if (has_offline_path) {
      if (resolver == "in-process" || resolver == "file") {
        resolver = "file";
      }
    }

    if (resolver == "file" && !has_offline_path) {
      g_state.config_error = true;
      g_state.config = std::nullopt;
      return;
    }

    bool explicit_port = g_state.pending_options.contains("port") ||
                         std::getenv("FLAGD_PORT") != nullptr ||
                         std::getenv("FLAGD_SYNC_PORT") != nullptr;
    if (!explicit_port) {
      if (resolver == "rpc") {
        config.SetPort(8013);
      } else if (resolver == "in-process") {
        config.SetPort(8015);
      }
    } else if (std::getenv("FLAGD_SYNC_PORT") != nullptr &&
               resolver == "in-process") {
      config.SetPort(std::stoi(std::getenv("FLAGD_SYNC_PORT")));
    }

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

    std::string fatal_codes_str;
    if (const char* env_codes = std::getenv("FLAGD_FATAL_STATUS_CODES")) {
      fatal_codes_str = env_codes;
    }
    auto fatal_it = g_state.pending_options.find("fatalStatusCodes");
    if (fatal_it != g_state.pending_options.end()) {
      fatal_codes_str = fatal_it->second;
    }
    g_state.fatal_status_codes_str = fatal_codes_str;

    g_state.resolved_resolver = resolver;
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
  CheckOptionValue(option, type, expected_val);
}

THEN(TheOptionOfTypeShouldHaveEmptyValue,
     "the option {string} of type {string} should have the value \"\"\"\"") {
  std::string option = CUKE_ARG(1);
  std::string type = CUKE_ARG(2);
  CheckOptionValue(option, type, "");
}

THEN(WeShouldHaveAnError, "we should have an error") {
  cuke::equal(g_state.config_error, true);
}

AFTER(CleanupEnv) {
  for (const auto& [var, val] : g_state.saved_env_vars) {
    if (val.has_value()) {
      setenv(var.c_str(), val->c_str(), 1);
    } else {
      unsetenv(var.c_str());
    }
  }
  g_state.saved_env_vars.clear();
  g_state.pending_options.clear();
  g_state.config.reset();
  g_state.config_error = false;
  g_state.resolved_resolver = "rpc";
  g_state.fatal_status_codes_str.clear();
}
