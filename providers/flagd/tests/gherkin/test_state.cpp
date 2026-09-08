#include "providers/flagd/tests/gherkin/test_state.h"

#include <cstdlib>

namespace openfeature::contrib::flagd::test {

TestState g_state;

void ResetTestState() {
  for (const auto& [var, val] : g_state.saved_env_vars) {
    if (val.has_value()) {
      setenv(var.c_str(), val->c_str(), 1);
    } else {
      unsetenv(var.c_str());
    }
  }
  g_state = TestState();
}

}  // namespace openfeature::contrib::flagd::test
