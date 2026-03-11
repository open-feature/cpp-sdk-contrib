
#ifndef CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_LOGIC_H_
#define CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_LOGIC_H_

#include <nlohmann/json.hpp>

#include "absl/status/statusor.h"
#include "json_logic.h"

namespace json_logic::ops {

absl::StatusOr<nlohmann::json> And(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data);
absl::StatusOr<nlohmann::json> Or(const JsonLogic& eval,
                                  const nlohmann::json& values,
                                  const nlohmann::json& data);
absl::StatusOr<nlohmann::json> Not(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data);
absl::StatusOr<nlohmann::json> DoubleNegation(const JsonLogic& eval,
                                              const nlohmann::json& values,
                                              const nlohmann::json& data);
absl::StatusOr<nlohmann::json> If(const JsonLogic& eval,
                                  const nlohmann::json& values,
                                  const nlohmann::json& data);
absl::StatusOr<nlohmann::json> StrictEquals(const JsonLogic& eval,
                                            const nlohmann::json& values,
                                            const nlohmann::json& data);
absl::StatusOr<nlohmann::json> StrictNotEquals(const JsonLogic& eval,
                                               const nlohmann::json& values,
                                               const nlohmann::json& data);

}  // namespace json_logic::ops

#endif  // CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_OPS_LOGIC_H_
