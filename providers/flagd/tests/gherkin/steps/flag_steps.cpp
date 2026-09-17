#include <string>

#include "defines.hpp"   // for GIVEN
#include "get_args.hpp"  // for CUKE_ARG
#include "providers/flagd/tests/gherkin/steps/step_utils.h"
#include "providers/flagd/tests/gherkin/test_context.h"

namespace {

using openfeature::contrib::flagd::test::Ctx;
using openfeature::contrib::flagd::test::FlagType;

void DeclareFlag(std::string key, FlagType type, std::string default_value) {
  auto& pending = Ctx().scenario.pending_eval;
  pending.declared = true;
  pending.flag_key = std::move(key);
  pending.flag_type = type;
  pending.default_value_str = std::move(default_value);
}

}  // namespace

// The testbed spells the same step two ways ("default value" and "fallback
// value") for every flag type; one macro keeps the ten definitions in sync.
#define GHERKIN_DECLARE_FLAG_STEPS(fn_prefix, type_name, flag_type)            \
  GIVEN(fn_prefix##WithDefault, "a " type_name                                 \
                                "-flag with key {string} and a default value " \
                                "{string}") {                                  \
    DeclareFlag(CUKE_ARG(1), flag_type, CUKE_ARG(2));                          \
  }                                                                            \
  GIVEN(fn_prefix##WithFallback,                                               \
        "a " type_name                                                         \
        "-flag with key {string} and a fallback value "                        \
        "{string}") {                                                          \
    DeclareFlag(CUKE_ARG(1), flag_type, CUKE_ARG(2));                          \
  }

GHERKIN_DECLARE_FLAG_STEPS(BooleanFlag, "Boolean", FlagType::kBoolean)
GHERKIN_DECLARE_FLAG_STEPS(StringFlag, "String", FlagType::kString)
GHERKIN_DECLARE_FLAG_STEPS(IntegerFlag, "Integer", FlagType::kInteger)
GHERKIN_DECLARE_FLAG_STEPS(FloatFlag, "Float", FlagType::kFloat)
GHERKIN_DECLARE_FLAG_STEPS(ObjectFlag, "Object", FlagType::kObject)

#undef GHERKIN_DECLARE_FLAG_STEPS
