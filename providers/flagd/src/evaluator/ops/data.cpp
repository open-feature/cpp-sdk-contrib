#include "data.h"

#include <algorithm>
#include <string>

#include "../evaluator.h"

namespace flagd::ops {

nlohmann::json Var(const Evaluator& eval, const nlohmann::json& values,
                   const nlohmann::json& data) {
  if (values.empty()) {
    return data;
  }

  nlohmann::json default_val = nullptr;
  if (values.size() > 1) {
    default_val = values[1];
  }

  std::string path;

  if (values[0].is_string()) {
    path = values[0].get<std::string>();
  } else if (values[0].is_null()) {
    return data;
  } else {
    return eval.Evaluate(default_val, data);
  }

  if (path.empty()) return data;

  std::replace(path.begin(), path.end(), '.', '/');
  std::string pointer_path = "/" + path;

  try {
    nlohmann::json::json_pointer ptr(pointer_path);
    if (data.contains(ptr)) {
      return data[ptr];
    }
  } catch (const nlohmann::json::exception&) {
    // Fallthrough to return default
  }

  return eval.Evaluate(default_val, data);
}

}  // namespace flagd::ops
