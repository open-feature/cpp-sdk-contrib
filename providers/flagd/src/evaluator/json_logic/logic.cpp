#include "logic.h"

#include "json_logic.h"
#include "utils.h"

namespace json_logic::ops {

absl::StatusOr<nlohmann::json> And(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data) {
  if (values.empty()) {
    return values;
  }

  nlohmann::json res;
  for (const auto& value : values) {
    auto applied = eval.Apply(value, data);
    if (!applied.ok()) {
      return applied.status();
    }
    res = applied.value();
    if (!Truthy(res)) {
      break;
    }
  }
  return res;
}

// TODO(#32): Implement the rest of Logic operators

}  // namespace json_logic::ops
