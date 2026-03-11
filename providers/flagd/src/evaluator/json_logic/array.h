
#ifndef CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_ARRAY_H_
#define CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_ARRAY_H_

#include <nlohmann/json.hpp>

#include "absl/status/statusor.h"
#include "json_logic.h"

namespace json_logic::ops {

absl::StatusOr<nlohmann::json> Merge(const JsonLogic& eval,
                                     const nlohmann::json& values,
                                     const nlohmann::json& data);

}  // namespace json_logic::ops

#endif  // CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_ARRAY_H_
