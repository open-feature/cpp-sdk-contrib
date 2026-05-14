#ifndef PROVIDERS_FLAGD_SRC_EVALUATOR_FLAGD_OPS_H_
#define PROVIDERS_FLAGD_SRC_EVALUATOR_FLAGD_OPS_H_

#include <nlohmann/json.hpp>

#include "absl/status/statusor.h"
#include "flagd/evaluator/json_logic/json_logic.h"

namespace flagd {

absl::StatusOr<nlohmann::json> StartsWith(const json_logic::JsonLogic& eval,
                                          const nlohmann::json& values,
                                          const nlohmann::json& data);
absl::StatusOr<nlohmann::json> EndsWith(const json_logic::JsonLogic& eval,
                                        const nlohmann::json& values,
                                        const nlohmann::json& data);
absl::StatusOr<nlohmann::json> SemVer(const json_logic::JsonLogic& eval,
                                      const nlohmann::json& values,
                                      const nlohmann::json& data);
absl::StatusOr<nlohmann::json> Fractional(const json_logic::JsonLogic& eval,
                                          const nlohmann::json& values,
                                          const nlohmann::json& data);

}  // namespace flagd

#endif  // PROVIDERS_FLAGD_SRC_EVALUATOR_FLAGD_OPS_H_
