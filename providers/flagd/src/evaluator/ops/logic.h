#ifndef CPP_SDK_FLAGD_EVALUATOR_OPS_LOGIC_H_
#define CPP_SDK_FLAGD_EVALUATOR_OPS_LOGIC_H_

#include <nlohmann/json.hpp>

#include "../evaluator.h"

namespace flagd::ops {

// Returns the first falsy argument, or the last argument if all are truthy.
nlohmann::json And(const Evaluator& eval, const nlohmann::json& values,
                   const nlohmann::json& data);

}  // namespace flagd::ops

#endif  // CPP_SDK_FLAGD_EVALUATOR_OPS_LOGIC_H_
