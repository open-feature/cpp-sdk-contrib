#include "data.h"

#include <algorithm>
#include <string>

#include "../json_logic.h"

namespace json_logic::ops {

namespace {
nlohmann::json::json_pointer CreateJsonPointer(std::string path) {
  std::replace(path.begin(), path.end(), '.', '/');
  return nlohmann::json::json_pointer("/" + path);
}
}  // namespace

nlohmann::json Var(const JsonLogic& eval, const nlohmann::json& values,
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
    return eval.Apply(default_val, data);
  }

  if (path.empty()) return data;

  try {
    auto ptr = CreateJsonPointer(path);
    if (data.contains(ptr)) {
      return data[ptr];
    }
  } catch (const nlohmann::json::exception&) {
    // Fallthrough to return default
  }

  return eval.Apply(default_val, data);
}

}  // namespace json_logic::ops
