#include "providers/flagd/tests/gherkin/test_context.h"

#include <stdlib.h>

#include <optional>
#include <string>
#include <utility>

namespace openfeature::contrib::flagd::test {

ScopedEnv::~ScopedEnv() { RestoreAll(); }

void ScopedEnv::Set(const std::string& name, const std::string& value) {
  if (!saved_.contains(name)) {
    const char* current = getenv(name.c_str());
    saved_.emplace(name, current != nullptr
                             ? std::optional<std::string>(current)
                             : std::nullopt);
  }
  setenv(name.c_str(), value.c_str(), 1);
}

void ScopedEnv::RestoreAll() {
  for (const auto& [name, value] : saved_) {
    if (value.has_value()) {
      setenv(name.c_str(), value->c_str(), 1);
    } else {
      unsetenv(name.c_str());
    }
  }
  saved_.clear();
}

TestContext& Ctx() {
  static TestContext* context = new TestContext();
  return *context;
}

void ResetScenarioState() {
  // Put the environment back before discarding the record of what was changed.
  Ctx().scenario.env.RestoreAll();

  Ctx().scenario.selector.clear();
  Ctx().scenario.targeting_key.clear();
  Ctx().scenario.context_attributes.clear();
  Ctx().scenario.nested_context_attributes.clear();
  Ctx().scenario.pending_eval = PendingEvaluation();
  Ctx().scenario.last_eval = EvaluationResult();
  Ctx().scenario.pending_options.clear();
  Ctx().scenario.config.reset();
  Ctx().scenario.config_error = false;
}

}  // namespace openfeature::contrib::flagd::test
