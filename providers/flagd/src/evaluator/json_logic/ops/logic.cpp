#include "logic.h"

#include "../json_logic.h"
#include "utils.h"

namespace json_logic::ops {

nlohmann::json And(const JsonLogic& eval, const nlohmann::json& values,
                   const nlohmann::json& data) {
  if (values.empty()) {
    return values;
  }

  nlohmann::json res;
  for (const auto& value : values) {
    res = eval.Apply(value, data);
    if (!Truthy(res)) {
      break;
    }
  }
  return res;
}

}  // namespace json_logic::ops
