
#ifndef CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_STRING_OPS_H_
#define CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_STRING_OPS_H_

#include <nlohmann/json.hpp>

#include "absl/status/statusor.h"
#include "json_logic.h"

namespace json_logic::ops {

absl::StatusOr<nlohmann::json> Cat(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data);
absl::StatusOr<nlohmann::json> Substr(const JsonLogic& eval,
                                      const nlohmann::json& values,
                                      const nlohmann::json& data);
absl::StatusOr<nlohmann::json> In(const JsonLogic& eval,
                                  const nlohmann::json& values,
                                  const nlohmann::json& data);

}  // namespace json_logic::ops

#endif  // CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_STRING_OPS_H_
