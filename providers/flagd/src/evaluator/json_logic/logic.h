#ifndef CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_LOGIC_H_
#define CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_LOGIC_H_

#include <nlohmann/json.hpp>

#include "json_logic.h"

namespace json_logic::ops {

// Returns the first falsy argument, or the last argument if all are truthy.
absl::StatusOr<nlohmann::json> And(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data);

}  // namespace json_logic::ops

#endif  // CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_LOGIC_H_
