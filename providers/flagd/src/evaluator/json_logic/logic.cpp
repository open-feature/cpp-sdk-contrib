#include "logic.h"

#include "absl/status/status.h"
#include "providers/flagd/src/evaluator/json_logic/utils.h"

namespace json_logic::ops {

namespace {
// Strict Truthy Check
absl::StatusOr<bool> GetBool(const JsonLogic& eval,
                             const nlohmann::json& values,
                             const nlohmann::json& data) {
  absl::StatusOr<nlohmann::json> result = eval.Apply(values, data);
  if (!result.ok()) return result.status();
  return Truthy(result.value());
}
}  // namespace

absl::StatusOr<nlohmann::json> And(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data) {
  nlohmann::json sugar = values;
  if (!values.is_array()) {
    sugar = nlohmann::json::array({values});
  }

  if (sugar.empty()) {
    return absl::InvalidArgumentError(
        "And with empty array is currently undefined behaviour");
  }

  nlohmann::json last_val;
  for (const nlohmann::json& val : sugar) {
    absl::StatusOr<nlohmann::json> res = eval.Apply(val, data);
    if (!res.ok()) return res.status();
    last_val = res.value();
    if (!Truthy(last_val)) return last_val;
  }
  return last_val;
}

absl::StatusOr<nlohmann::json> Or(const JsonLogic& eval,
                                  const nlohmann::json& values,
                                  const nlohmann::json& data) {
  nlohmann::json sugar = values;
  if (!values.is_array()) {
    sugar = nlohmann::json::array({values});
  }

  if (sugar.empty()) {
    return absl::InvalidArgumentError(
        "Or with empty array is currently undefined behaviour");
  }

  nlohmann::json last_val;
  for (const nlohmann::json& val : sugar) {
    absl::StatusOr<nlohmann::json> res = eval.Apply(val, data);
    if (!res.ok()) return res.status();
    last_val = res.value();
    if (Truthy(last_val)) return last_val;
  }
  return last_val;
}

absl::StatusOr<nlohmann::json> Not(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data) {
  if (values.is_array() && values.size() > 1) {
    return absl::InvalidArgumentError("! accepts only single argument.");
  }
  nlohmann::json sugar = values.is_array() ? values[0] : values;
  absl::StatusOr<bool> res = GetBool(eval, sugar, data);
  if (!res.ok()) return res.status();
  return !res.value();
}

absl::StatusOr<nlohmann::json> DoubleNegation(const JsonLogic& eval,
                                              const nlohmann::json& values,
                                              const nlohmann::json& data) {
  if (values.is_array() && values.size() > 1) {
    return absl::InvalidArgumentError("!! accepts only single argument.");
  }
  nlohmann::json sugar = values.is_array() ? values[0] : values;
  return GetBool(eval, sugar, data);
}

absl::StatusOr<nlohmann::json> If(const JsonLogic& eval,
                                  const nlohmann::json& values,
                                  const nlohmann::json& data) {
  if (!values.is_array() || values.empty()) {
    return absl::InvalidArgumentError(
        "`If` operator accepts only non-empty array argument");
  }

  for (size_t i = 0; i < values.size() - 1; i += 2) {
    absl::StatusOr<bool> check = GetBool(eval, values[i], data);
    if (!check.ok()) return check.status();
    if (*check) {
      return eval.Apply(values[i + 1], data);
    }
  }
  if (values.size() % 2 == 1) {
    return eval.Apply(values.back(), data);
  }
  return absl::InvalidArgumentError(
      "No `if` operator rule matched and no default provided");
}

absl::StatusOr<nlohmann::json> StrictEquals(const JsonLogic& eval,
                                            const nlohmann::json& values,
                                            const nlohmann::json& data) {
  if (!values.is_array() || values.size() < 2) return false;

  absl::StatusOr<nlohmann::json> left = eval.Apply(values[0], data);
  if (!left.ok()) return left.status();

  absl::StatusOr<nlohmann::json> right = eval.Apply(values[1], data);
  if (!right.ok()) return right.status();

  return *left == *right;
}

absl::StatusOr<nlohmann::json> StrictNotEquals(const JsonLogic& eval,
                                               const nlohmann::json& values,
                                               const nlohmann::json& data) {
  if (!values.is_array() || values.size() < 2) return true;

  absl::StatusOr<nlohmann::json> left = eval.Apply(values[0], data);
  if (!left.ok()) return left.status();

  absl::StatusOr<nlohmann::json> right = eval.Apply(values[1], data);
  if (!right.ok()) return right.status();

  return *left != *right;
}

}  // namespace json_logic::ops
