#include "data.h"

#include <algorithm>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "json_logic.h"

namespace json_logic::ops {

namespace {

nlohmann::json::json_pointer CreateJsonPointer(std::string path) {
  std::replace(path.begin(), path.end(), '.', '/');
  return nlohmann::json::json_pointer("/" + path);
}

absl::StatusOr<nlohmann::json> GetVariableValue(const nlohmann::json& data,
                                                const std::string& key) {
  try {
    nlohmann::json::json_pointer ptr = CreateJsonPointer(key);
    if (data.contains(ptr)) {
      return data[ptr];
    }
  } catch (...) {
    // invalid pointer or path not found
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "Var key '", key, "' is missing from object: ", data.dump()));
}

}  // namespace

absl::StatusOr<nlohmann::json> Var(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data) {
  std::optional<nlohmann::json> default_val;
  nlohmann::json key_object;
  std::string key;

  if (values.is_array()) {
    if (!values.empty()) {
      absl::StatusOr<nlohmann::json> resolved_key = eval.Apply(values[0], data);
      if (!resolved_key.ok()) return resolved_key;
      key_object = resolved_key.value();
    }
    if (values.size() > 1) {
      default_val = values[1];
    }
    if (values.size() > 2) {
      return absl::InvalidArgumentError("Var requires up to 2 arguments.");
    }
  } else {
    key_object = values;
  }

  if (key_object.is_null()) {
    return data;
  }

  if (!key_object.is_string()) {
    return absl::InvalidArgumentError("Var key must be a string.");
  }

  key = key_object.get<std::string>();

  if (key.empty()) return data;

  absl::StatusOr<nlohmann::json> result = GetVariableValue(data, key);
  if (result.ok()) return result.value();

  if (default_val.has_value()) {
    return eval.Apply(default_val.value(), data);
  }

  return nlohmann::json(nullptr);
}

absl::StatusOr<nlohmann::json> Missing(const JsonLogic& eval,
                                       const nlohmann::json& values,
                                       const nlohmann::json& data) {
  absl::StatusOr<nlohmann::json> resolved_args_res = eval.Apply(values, data);
  if (!resolved_args_res.ok()) {
    return resolved_args_res;
  }
  nlohmann::json resolved_args = resolved_args_res.value();

  if (!resolved_args.is_array()) {
    resolved_args = nlohmann::json::array({resolved_args});
  }

  nlohmann::json missing = nlohmann::json::array();
  for (const nlohmann::json& arg : resolved_args) {
    if (!arg.is_string()) {
      return absl::InvalidArgumentError("Missing key must be a string.");
    }

    absl::StatusOr<nlohmann::json> result =
        GetVariableValue(data, arg.get<std::string>());

    if (!result.ok()) {
      missing.push_back(arg);
    }
  }
  return missing;
}

absl::StatusOr<nlohmann::json> MissingSome(const JsonLogic& eval,
                                           const nlohmann::json& values,
                                           const nlohmann::json& data) {
  if (!values.is_array() || values.size() != 2) {
    return absl::InvalidArgumentError(
        "MissingSome requires exactly two arguments.");
  }

  absl::StatusOr<nlohmann::json> resolved_min_res = eval.Apply(values[0], data);
  if (!resolved_min_res.ok()) return resolved_min_res;
  const nlohmann::json& resolved_min = *resolved_min_res;

  if (!resolved_min.is_number_unsigned()) {
    return absl::InvalidArgumentError(
        "MissingSome min_required must be an unsigned integer.");
  }
  uint64_t min_required = resolved_min.get<uint64_t>();

  const nlohmann::json& keys_logic = values[1];
  absl::StatusOr<nlohmann::json> resolved_keys_res =
      eval.Apply(keys_logic, data);
  if (!resolved_keys_res.ok()) return resolved_keys_res;
  const nlohmann::json& keys = *resolved_keys_res;

  if (!keys.is_array()) {
    return absl::InvalidArgumentError("MissingSome keys must be an array.");
  }

  absl::StatusOr<nlohmann::json> missing_res = Missing(eval, keys, data);
  if (!missing_res.ok()) return missing_res.status();

  nlohmann::json missing = *missing_res;

  // As we know that `missing` is a subset of keys, we can safely subtract the
  // sizes.
  if (keys.size() - missing.size() >= min_required) {
    return nlohmann::json::array();
  }

  return missing;
}

}  // namespace json_logic::ops
