
#include "string_ops.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_join.h"

namespace json_logic::ops {

absl::StatusOr<nlohmann::json> Cat(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data) {
  absl::StatusOr<nlohmann::json> resolved_values_res = eval.Apply(values, data);
  if (!resolved_values_res.ok()) return resolved_values_res;
  const nlohmann::json& resolved_values = *resolved_values_res;

  if (resolved_values.is_string()) {
    return resolved_values;
  }
  if (!resolved_values.is_array()) {
    if (resolved_values.is_primitive()) {
      return absl::InvalidArgumentError("cat with non-string primitive");
    }
    return resolved_values;
  }

  std::vector<std::string_view> pieces;
  pieces.reserve(resolved_values.size());
  for (const nlohmann::json& val : resolved_values) {
    if (!val.is_string()) {
      return absl::InvalidArgumentError("cat with non-string argument");
    }
    pieces.push_back(val.get_ref<const std::string&>());
  }
  return absl::StrJoin(pieces, "");
}

absl::StatusOr<nlohmann::json> Substr(const JsonLogic& eval,
                                      const nlohmann::json& values,
                                      const nlohmann::json& data) {
  absl::StatusOr<nlohmann::json> resolved_values_res = eval.Apply(values, data);
  if (!resolved_values_res.ok()) return resolved_values_res;
  const nlohmann::json& resolved_values = *resolved_values_res;

  if (!resolved_values.is_array() || resolved_values.empty()) {
    return absl::InvalidArgumentError(
        "Arguments to substr must be a non-empty array");
  }

  if (!resolved_values[0].is_string()) {
    return absl::InvalidArgumentError(
        "prop argument to substr must be a string");
  }

  std::string str = resolved_values[0].get<std::string>();
  int64_t start = 0;

  if (resolved_values.size() > 1) {
    if (!resolved_values[1].is_number_integer()) {
      return absl::InvalidArgumentError(
          "Start argument to substr must be an integer");
    }
    start = resolved_values[1].get<int64_t>();
  }

  if (start < 0) {
    start = str.length() + start;
    start = std::max(start, static_cast<int64_t>(0));
  }

  if (static_cast<size_t>(start) >= str.length()) return "";

  if (resolved_values.size() > 2) {
    if (!resolved_values[2].is_number_integer()) {
      return absl::InvalidArgumentError(
          "Length argument to substr must be an integer");
    }
    int len = resolved_values[2].get<int>();
    if (len < 0) {
      len = str.length() - start + len;
    }
    if (len < 0) return "";
    return str.substr(start, len);
  }

  return str.substr(start);
}

absl::StatusOr<nlohmann::json> In(const JsonLogic& eval,
                                  const nlohmann::json& values,
                                  const nlohmann::json& data) {
  if (!values.is_array() || values.size() != 2) {
    return absl::InvalidArgumentError(
        "Operator `in` requires exactly two arguments.");
  }

  absl::StatusOr<nlohmann::json> sub_res = eval.Apply(values[0], data);
  if (!sub_res.ok()) return sub_res;
  const nlohmann::json& sub = *sub_res;

  absl::StatusOr<nlohmann::json> container_res = eval.Apply(values[1], data);
  if (!container_res.ok()) return container_res;
  const nlohmann::json& container = *container_res;

  if (container.is_string()) {
    if (!sub.is_string()) {
      return absl::InvalidArgumentError(
          "in operator: substring check requires string");
    }
    return container.get<std::string>().find(sub.get<std::string>()) !=
           std::string::npos;
  }

  if (container.is_array()) {
    for (const nlohmann::json& item : container) {
      if (item == sub) return true;
    }
    return false;
  }

  return absl::InvalidArgumentError(
      "in operator: container must be string or array");
}

}  // namespace json_logic::ops
