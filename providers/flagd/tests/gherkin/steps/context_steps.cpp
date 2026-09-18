#include <string>

#include "defines.hpp"   // for GIVEN
#include "get_args.hpp"  // for CUKE_ARG
#include "openfeature/value.h"
#include "providers/flagd/tests/gherkin/steps/step_utils.h"
#include "providers/flagd/tests/gherkin/test_context.h"

namespace {

using openfeature::contrib::flagd::test::Ctx;
using openfeature::contrib::flagd::test::FailStep;
using openfeature::contrib::flagd::test::ParseBool;
using openfeature::contrib::flagd::test::ParseDouble;
using openfeature::contrib::flagd::test::ParseInt64;

void AddContextAttribute(const std::string& key, const std::string& type,
                         const std::string& value) {
  if (key == "targetingKey") {
    Ctx().scenario.targeting_key = value;
    return;
  }

  auto& attributes = Ctx().scenario.context_attributes;
  if (type == "String") {
    attributes[key] = value;
  } else if (type == "Boolean") {
    if (auto parsed = ParseBool(value)) {
      attributes[key] = *parsed;
    } else {
      FailStep("context attribute '" + key + "' is not a valid Boolean: '" +
               value + "'");
    }
  } else if (type == "Integer") {
    if (auto parsed = ParseInt64(value)) {
      attributes[key] = *parsed;
    } else {
      FailStep("context attribute '" + key + "' is not a valid Integer: '" +
               value + "'");
    }
  } else if (type == "Float") {
    if (auto parsed = ParseDouble(value)) {
      attributes[key] = *parsed;
    } else {
      FailStep("context attribute '" + key + "' is not a valid Float: '" +
               value + "'");
    }
  } else {
    FailStep("unsupported context attribute type '" + type + "' for key '" +
             key + "'");
  }
}

}  // namespace

GIVEN(AContextContainingKeyTypeValue,
      "a context containing a key {string}, with type {string} and with value "
      "{string}") {
  AddContextAttribute(CUKE_ARG(1), CUKE_ARG(2), CUKE_ARG(3));
}

GIVEN(AContextContainingKeyTypeEmptyValue,
      "a context containing a key {string}, with type {string} and with value "
      "" GHERKIN_EMPTY_ARG) {
  AddContextAttribute(CUKE_ARG(1), CUKE_ARG(2), "");
}

GIVEN(AContextContainingTargetingKey,
      "a context containing a targeting key with value {string}") {
  Ctx().scenario.targeting_key = static_cast<std::string>(CUKE_ARG(1));
}

GIVEN(AContextContainingNestedProperty,
      "a context containing a nested property with outer key {string} and "
      "inner key {string}, with value {string}") {
  const std::string outer_key = CUKE_ARG(1);
  const std::string inner_key = CUKE_ARG(2);
  const std::string value = CUKE_ARG(3);
  Ctx().scenario.nested_context_attributes[outer_key][inner_key] =
      ::openfeature::Value(value);
}
