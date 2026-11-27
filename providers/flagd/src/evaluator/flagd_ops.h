#ifndef PROVIDERS_FLAGD_SRC_EVALUATOR_FLAGD_OPS_H_
#define PROVIDERS_FLAGD_SRC_EVALUATOR_FLAGD_OPS_H_

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

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
absl::StatusOr<nlohmann::json> Resolve(const json_logic::JsonLogic& eval,
                                       const nlohmann::json& values,
                                       const nlohmann::json& data);

// binary subtype of a resolve result; only the resolve operator creates one
inline constexpr std::uint8_t kResolveSubtype = 0x52;

struct ResolveResult {
  std::optional<std::string> variant;  // nullopt for a null variant
  std::optional<std::string> reason;   // nullopt keeps the default reason
  nlohmann::json metadata = nlohmann::json::object();
};

bool IsResolveResult(const nlohmann::json& value);
std::optional<ResolveResult> DecodeResolveResult(const nlohmann::json& value);

}  // namespace flagd

#endif  // PROVIDERS_FLAGD_SRC_EVALUATOR_FLAGD_OPS_H_
