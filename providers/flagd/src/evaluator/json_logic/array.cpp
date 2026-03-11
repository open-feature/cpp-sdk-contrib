
#include "array.h"

namespace json_logic::ops {

absl::StatusOr<nlohmann::json> Merge(const JsonLogic& eval,
                                     const nlohmann::json& values,
                                     const nlohmann::json& data) {
  absl::StatusOr<nlohmann::json> resolved_values_res = eval.Apply(values, data);
  if (!resolved_values_res.ok()) return resolved_values_res;
  const nlohmann::json& resolved_values = *resolved_values_res;

  if (!resolved_values.is_array()) {
    return nlohmann::json::array({resolved_values});
  }

  nlohmann::json result = nlohmann::json::array();
  for (const nlohmann::json& val : resolved_values) {
    if (val.is_array()) {
      result.insert(result.end(), val.begin(), val.end());
    } else {
      result.push_back(val);
    }
  }
  return result;
}

}  // namespace json_logic::ops
