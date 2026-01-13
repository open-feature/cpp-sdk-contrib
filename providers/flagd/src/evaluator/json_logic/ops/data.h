#ifndef CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_DATA_H_
#define CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_DATA_H_

#include <nlohmann/json.hpp>

#include "../json_logic.h"

namespace json_logic::ops {

// Retrieve data using dot notation or array indices (e.g. "a.b.1"). If the path
// is missing, returns the second argument (if present) or null.
nlohmann::json Var(const JsonLogic& eval, const nlohmann::json& values,
                   const nlohmann::json& data);

}  // namespace json_logic::ops

#endif  // CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_DATA_H_
