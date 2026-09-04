#include <string>

#include "defines.hpp"   // for GIVEN
#include "get_args.hpp"  // for CUKE_ARG
#include "providers/flagd/tests/gherkin/test_state.h"

using openfeature::contrib::flagd::test::g_state;

GIVEN(ABooleanFlag,
      "a Boolean-flag with key {string} and a default value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Boolean";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AStringFlag,
      "a String-flag with key {string} and a default value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "String";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AIntegerFlag,
      "a Integer-flag with key {string} and a default value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Integer";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AFloatFlag,
      "a Float-flag with key {string} and a default value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Float";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AnObjectFlag,
      "a Object-flag with key {string} and a default value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Object";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(ABooleanFlagFallback,
      "a Boolean-flag with key {string} and a fallback value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Boolean";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AStringFlagFallback,
      "a String-flag with key {string} and a fallback value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "String";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AIntegerFlagFallback,
      "a Integer-flag with key {string} and a fallback value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Integer";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AFloatFlagFallback,
      "a Float-flag with key {string} and a fallback value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Float";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}

GIVEN(AnObjectFlagFallback,
      "a Object-flag with key {string} and a fallback value {string}") {
  g_state.last_eval.flag_key = static_cast<std::string>(CUKE_ARG(1));
  g_state.last_eval.flag_type = "Object";
  g_state.last_eval.default_value_str = static_cast<std::string>(CUKE_ARG(2));
}
