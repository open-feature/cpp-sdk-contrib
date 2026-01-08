#ifndef CPP_SDK_FLAGD_EVALUATOR_OPS_DATA_H_
#define CPP_SDK_FLAGD_EVALUATOR_OPS_DATA_H_

#include <nlohmann/json.hpp>

#include "../evaluator.h"

namespace flagd::ops {

// Retrieve data using dot notation or array indices (e.g. "a.b.1"). If the path
// is missing, returns the second argument (if present) or null.
nlohmann::json Var(const Evaluator& eval, const nlohmann::json& values,
                   const nlohmann::json& data);

}  // namespace flagd::ops

#endif  // CPP_SDK_FLAGD_EVALUATOR_OPS_DATA_H_
