#ifndef CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_UTILS_H_
#define CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_UTILS_H_

#include <nlohmann/json.hpp>

namespace json_logic::ops {

// Determines if a JSON value is "truthy" according to JsonLogic rules.
// Falsy values: false, 0, [] (empty array), "" (empty string), null.
// All other values are truthy (including empty objects).
bool Truthy(const nlohmann::json& value);

}  // namespace json_logic::ops

#endif  // CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_UTILS_H_
