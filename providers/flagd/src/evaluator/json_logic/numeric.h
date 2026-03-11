#ifndef CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_NUMERIC_H_
#define CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_NUMERIC_H_

#include <nlohmann/json.hpp>

#include "absl/status/statusor.h"
#include "json_logic.h"

namespace json_logic::ops {

absl::StatusOr<nlohmann::json> Min(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data);
absl::StatusOr<nlohmann::json> Max(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data);
absl::StatusOr<nlohmann::json> Add(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data);
absl::StatusOr<nlohmann::json> Subtract(const JsonLogic& eval,
                                        const nlohmann::json& values,
                                        const nlohmann::json& data);
absl::StatusOr<nlohmann::json> Multiply(const JsonLogic& eval,
                                        const nlohmann::json& values,
                                        const nlohmann::json& data);
absl::StatusOr<nlohmann::json> Divide(const JsonLogic& eval,
                                      const nlohmann::json& values,
                                      const nlohmann::json& data);
absl::StatusOr<nlohmann::json> Modulo(const JsonLogic& eval,
                                      const nlohmann::json& values,
                                      const nlohmann::json& data);
absl::StatusOr<nlohmann::json> LessThan(const JsonLogic& eval,
                                        const nlohmann::json& values,
                                        const nlohmann::json& data);
absl::StatusOr<nlohmann::json> LessThanOrEqual(const JsonLogic& eval,
                                               const nlohmann::json& values,
                                               const nlohmann::json& data);
absl::StatusOr<nlohmann::json> GreaterThan(const JsonLogic& eval,
                                           const nlohmann::json& values,
                                           const nlohmann::json& data);
absl::StatusOr<nlohmann::json> GreaterThanOrEqual(const JsonLogic& eval,
                                                  const nlohmann::json& values,
                                                  const nlohmann::json& data);

}  // namespace json_logic::ops

#endif  // CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_NUMERIC_H_
