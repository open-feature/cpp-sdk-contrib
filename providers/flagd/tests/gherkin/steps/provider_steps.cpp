#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "defines.hpp"  // for GIVEN
#include "flagd/configuration.h"
#include "flagd/provider.h"
#include "get_args.hpp"  // for CUKE_ARG
#include "openfeature/openfeature_api.h"
#include "providers/flagd/tests/gherkin/steps/step_utils.h"
#include "providers/flagd/tests/gherkin/test_env.h"
#include "providers/flagd/tests/gherkin/test_state.h"

using openfeature::contrib::flagd::test::g_current_selector;
using openfeature::contrib::flagd::test::g_stable_provider;
using openfeature::contrib::flagd::test::g_state;

namespace {

void InitializeProvider() {
  if (g_stable_provider && g_state.selector == g_current_selector) {
    g_state.provider = g_stable_provider;
    return;
  }

  if (!openfeature::contrib::flagd::test::WaitForGrpcReady("localhost:8015")) {
    std::cerr << "WARNING: Flagd gRPC service not ready on port 8015\n";
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
}

}  // namespace

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

GIVEN(AStableFlagdProvider, "a stable flagd provider") { InitializeProvider(); }

GIVEN(AMetadataFlagdProvider, "a metadata flagd provider") {
  InitializeProvider();
}

GIVEN(AnEvaluator, "an evaluator") { InitializeProvider(); }
