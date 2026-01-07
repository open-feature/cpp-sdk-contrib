#include "providers/flagd/src/evaluator/ops/utils.h"

#include <string>

namespace flagd::ops {

bool Truthy(const nlohmann::json& value) {
  if (value.is_null()) {
    return false;
  }
  if (value.is_boolean()) {
    return value.get<bool>();
  }
  if (value.is_number()) {
    if (value.is_number_float()) {
      return value.get<double>() != 0.0;
    }
    // Integers and Unsigned
    return value != 0;
  }
  if (value.is_string()) {
    return !value.get<std::string>().empty();
  }
  if (value.is_array()) {
    return !value.empty();
  }
  // Objects and anything else are truthy
  return true;
}

}  // namespace flagd::ops
