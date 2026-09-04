#include "providers/flagd/tests/gherkin/test_state.h"

namespace openfeature::contrib::flagd::test {

TestState g_state;

void ResetTestState() { g_state = TestState(); }

}  // namespace openfeature::contrib::flagd::test
