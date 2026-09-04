#include "defines.hpp"  // for BEFORE, AFTER
#include "providers/flagd/tests/gherkin/test_env.h"
#include "providers/flagd/tests/gherkin/test_state.h"

using openfeature::contrib::flagd::test::ResetTestState;
using openfeature::contrib::flagd::test::SetupGlobalFlagd;

BEFORE(SetupFlagd) {
  ResetTestState();
  SetupGlobalFlagd();
}

AFTER(CleanupFlagd) {
  // Do not stop global flagd between scenarios
}
