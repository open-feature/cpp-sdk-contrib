#include <cstdlib>
#include <iostream>
#include <string>

#include "defines.hpp"  // for BEFORE_ALL, BEFORE, AFTER_ALL
#include "providers/flagd/tests/gherkin/test_context.h"
#include "providers/flagd/tests/gherkin/test_env.h"

namespace {

using openfeature::contrib::flagd::test::ResetScenarioState;
using openfeature::contrib::flagd::test::SetupGlobalFlagd;
using openfeature::contrib::flagd::test::TeardownGlobalFlagd;

}  // namespace

// flagd is shared by every scenario, so it belongs in BEFORE_ALL rather than
// in BEFORE behind a "have I already done this?" guard.
BEFORE_ALL(StartFlagd) {
  std::string error;
  if (!SetupGlobalFlagd(&error)) {
    // BEFORE_ALL cannot fail a run, and continuing would report every scenario
    // as a provider bug rather than an environment problem.
    std::cerr << "CRITICAL: could not prepare the flagd test environment: "
              << error << '\n';
    std::exit(1);
  }
}

AFTER_ALL(StopFlagd) { TeardownGlobalFlagd(); }

BEFORE(ResetScenario) { ResetScenarioState(); }
