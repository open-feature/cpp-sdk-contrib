#include "logic.h"

#include "../evaluator.h"
#include "utils.h"

namespace flagd::ops {

nlohmann::json And(const Evaluator& eval, const nlohmann::json& values,
                   const nlohmann::json& data) {
  if (values.empty()) {
    return values;
  }

  nlohmann::json res;
  for (const auto& value : values) {
    res = eval.Evaluate(value, data);
    if (!Truthy(res)) {
      break;
    }
  }
  return res;
}

}  // namespace flagd::ops
