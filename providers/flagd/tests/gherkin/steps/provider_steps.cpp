#include <iostream>
#include <memory>
#include <string>

#include "defines.hpp"  // for GIVEN
#include "flagd/configuration.h"
#include "flagd/provider.h"
#include "get_args.hpp"  // for CUKE_ARG
#include "openfeature/openfeature_api.h"
#include "providers/flagd/tests/gherkin/steps/step_utils.h"
#include "providers/flagd/tests/gherkin/test_context.h"
#include "providers/flagd/tests/gherkin/test_env.h"

namespace {

using openfeature::contrib::flagd::test::Ctx;
using openfeature::contrib::flagd::test::FailStep;
using openfeature::contrib::flagd::test::FlagdSyncTarget;
using openfeature::contrib::flagd::test::kFlagdSyncPort;
using openfeature::contrib::flagd::test::WaitForGrpcReady;

void InitializeProvider() {
  auto& persistent = Ctx().persistent;
  const std::string& selector = Ctx().scenario.selector;

  // Reuse the provider when nothing about it would change; building one costs
  // a gRPC connection and a full sync handshake.
  if (persistent.provider_registered && persistent.provider != nullptr &&
      persistent.provider_selector == selector) {
    return;
  }

  if (!WaitForGrpcReady(FlagdSyncTarget())) {
    FailStep("flagd sync service is not reachable on " + FlagdSyncTarget() +
             "\nlast lines of the flagd log:\n" +
             openfeature::contrib::flagd::test::GlobalFlagdLogTail());
    return;
  }

  ::flagd::FlagdProviderConfig config;
  config.SetHost("localhost");
  config.SetPort(kFlagdSyncPort);
  config.SetDeadlineMs(5000);
  if (!selector.empty()) {
    config.SetSelector(selector);
  }

  auto provider = std::make_shared<::flagd::FlagdProvider>(config);
  ::openfeature::OpenFeatureAPI::GetInstance().SetProviderAndWait(provider);

  // Cache only after the API accepts the provider; caching earlier would let
  // later scenarios reuse a provider that was never registered.
  persistent.provider = std::move(provider);
  persistent.provider_selector = selector;
  persistent.provider_registered = true;
}

}  // namespace

GIVEN(AnOptionOfTypeWithValue,
      "an option {string} of type {string} with value {string}") {
  const std::string option = CUKE_ARG(1);
  const std::string value = CUKE_ARG(3);
  Ctx().scenario.pending_options[option] = value;
  if (option == "selector") {
    Ctx().scenario.selector = value;
  }
}

GIVEN(AStableFlagdProvider, "a stable flagd provider") { InitializeProvider(); }

GIVEN(AMetadataFlagdProvider, "a metadata flagd provider") {
  InitializeProvider();
}

GIVEN(AnEvaluator, "an evaluator") { InitializeProvider(); }
