#include <cstdlib>
#include <string>

#include "asserts.hpp"  // for cuke::equal
#include "defines.hpp"  // for GIVEN, WHEN, THEN, AFTER
#include "flagd/configuration.h"
#include "get_args.hpp"  // for CUKE_ARG
#include "providers/flagd/tests/gherkin/test_state.h"

using openfeature::contrib::flagd::test::g_state;

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
