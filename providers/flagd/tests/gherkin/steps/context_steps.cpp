#include <cstdint>
#include <string>

#include "defines.hpp"   // for GIVEN
#include "get_args.hpp"  // for CUKE_ARG
#include "openfeature/value.h"
#include "providers/flagd/tests/gherkin/test_state.h"

using openfeature::contrib::flagd::test::g_state;

GIVEN(AContextContainingKeyTypeValue,
      "a context containing a key {string}, with type {string} and with value "
      "{string}") {
  std::string key = CUKE_ARG(1);
  std::string type = CUKE_ARG(2);
  std::string value = CUKE_ARG(3);

  if (key == "targetingKey") {
    g_state.targeting_key = value;
  } else {
    if (type == "String") {
      g_state.context_attributes[key] = value;
    } else if (type == "Boolean") {
      g_state.context_attributes[key] = (value == "true" || value == "True");
    } else if (type == "Integer") {
      g_state.context_attributes[key] = static_cast<int64_t>(std::stoll(value));
    } else if (type == "Float") {
      g_state.context_attributes[key] = std::stod(value);
    }
  }
}

GIVEN(AContextContainingTargetingKey,
      "a context containing a targeting key with value {string}") {
  g_state.targeting_key = static_cast<std::string>(CUKE_ARG(1));
}

GIVEN(AContextContainingNestedProperty,
      "a context containing a nested property with outer key {string} and "
      "inner key {string}, with value {string}") {
  std::string outer_key = CUKE_ARG(1);
  std::string inner_key = CUKE_ARG(2);
  std::string value = CUKE_ARG(3);

  g_state.nested_context_attributes[outer_key][inner_key] =
      ::openfeature::Value(value);
}
