#include "numeric.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <type_traits>
#include <variant>
#include <vector>

#include "absl/status/status.h"

namespace json_logic::ops {

namespace {

/**
 * A robust representation of a JSON number supporting various precisions.
 * Preserves integer precision where possible, following C++ promotion rules.
 */
class Number {
 public:
  explicit Number(const nlohmann::json& json_val) {
    if (json_val.is_number_unsigned()) {
      value_ = json_val.get<uint64_t>();
    } else if (json_val.is_number_integer()) {
      value_ = json_val.get<int64_t>();
    } else {
      value_ = json_val.get<double>();
    }
  }

  explicit Number(int64_t val) : value_(val) {}
  explicit Number(uint64_t val) : value_(val) {}
  explicit Number(double val) : value_(val) {}

  double AsDouble() const noexcept {
    return std::visit([](auto val) { return static_cast<double>(val); },
                      value_);
  }

  int64_t AsInt64() const noexcept {
    return std::visit([](auto val) { return static_cast<int64_t>(val); },
                      value_);
  }

  bool IsFloat() const noexcept {
    return std::holds_alternative<double>(value_);
  }

  nlohmann::json ToJson() const {
    return std::visit([](auto val) { return nlohmann::json(val); }, value_);
  }

  bool operator<(const Number& other) const noexcept {
    return std::visit(
        [](auto lhs_val, auto rhs_val) -> bool {
          using LeftType = decltype(lhs_val);
          using RightType = decltype(rhs_val);
          if constexpr (std::is_integral_v<LeftType> &&
                        std::is_integral_v<RightType>) {
            if constexpr (std::is_signed_v<LeftType> ==
                          std::is_signed_v<RightType>) {
              return lhs_val < rhs_val;
            } else if constexpr (std::is_signed_v<LeftType>) {
              // lhs is signed, rhs is unsigned
              return lhs_val < 0 || static_cast<uint64_t>(lhs_val) < rhs_val;
            } else {
              // lhs is unsigned, rhs is signed
              return rhs_val >= 0 && lhs_val < static_cast<uint64_t>(rhs_val);
            }
          }
          return static_cast<double>(lhs_val) < static_cast<double>(rhs_val);
        },
        value_, other.value_);
  }

  bool operator==(const Number& other) const noexcept {
    return std::visit(
        [](auto lhs_val, auto rhs_val) -> bool {
          using LeftType = decltype(lhs_val);
          using RightType = decltype(rhs_val);
          if constexpr (std::is_integral_v<LeftType> &&
                        std::is_integral_v<RightType>) {
            if constexpr (std::is_signed_v<LeftType> ==
                          std::is_signed_v<RightType>) {
              return lhs_val == rhs_val;
            } else if constexpr (std::is_signed_v<LeftType>) {
              return lhs_val >= 0 && static_cast<uint64_t>(lhs_val) == rhs_val;
            } else {
              return rhs_val >= 0 && lhs_val == static_cast<uint64_t>(rhs_val);
            }
          }
          return static_cast<double>(lhs_val) == static_cast<double>(rhs_val);
        },
        value_, other.value_);
  }

  static Number Add(const Number& lhs, const Number& rhs) noexcept {
    return std::visit(
        [](auto lhs_val, auto rhs_val) -> Number {
          if constexpr (std::is_integral_v<decltype(lhs_val)> &&
                        std::is_integral_v<decltype(rhs_val)>) {
            return Number(lhs_val + rhs_val);
          }
          return Number(static_cast<double>(lhs_val) +
                        static_cast<double>(rhs_val));
        },
        lhs.value_, rhs.value_);
  }

  static Number Mul(const Number& lhs, const Number& rhs) noexcept {
    return std::visit(
        [](auto lhs_val, auto rhs_val) -> Number {
          if constexpr (std::is_integral_v<decltype(lhs_val)> &&
                        std::is_integral_v<decltype(rhs_val)>) {
            return Number(lhs_val * rhs_val);
          }
          return Number(static_cast<double>(lhs_val) *
                        static_cast<double>(rhs_val));
        },
        lhs.value_, rhs.value_);
  }

  static Number Sub(const Number& lhs, const Number& rhs) noexcept {
    return std::visit(
        [](auto lhs_val, auto rhs_val) -> Number {
          using LeftType = decltype(lhs_val);
          using RightType = decltype(rhs_val);
          if constexpr (std::is_integral_v<LeftType> &&
                        std::is_integral_v<RightType>) {
            if ((!std::is_unsigned_v<LeftType> ||
                 lhs_val <= static_cast<uint64_t>(
                                std::numeric_limits<int64_t>::max())) &&
                (!std::is_unsigned_v<RightType> ||
                 rhs_val <= static_cast<uint64_t>(
                                std::numeric_limits<int64_t>::max()))) {
              return Number(static_cast<int64_t>(lhs_val) -
                            static_cast<int64_t>(rhs_val));
            }

            // Fallback for very large unsigned values that don't fit in int64_t
            if constexpr (std::is_same_v<LeftType, uint64_t> &&
                          std::is_same_v<RightType, uint64_t>) {
              if (lhs_val >= rhs_val) return Number(lhs_val - rhs_val);
            }

            return Number(static_cast<double>(lhs_val) -
                          static_cast<double>(rhs_val));
          }
          return Number(static_cast<double>(lhs_val) -
                        static_cast<double>(rhs_val));
        },
        lhs.value_, rhs.value_);
  }

  static absl::StatusOr<Number> Div(const Number& lhs,
                                    const Number& rhs) noexcept {
    double divisor = rhs.AsDouble();
    if (divisor == 0.0) {
      return absl::InvalidArgumentError("Division by zero");
    }
    return Number(lhs.AsDouble() / divisor);
  }

  static absl::StatusOr<Number> Mod(const Number& lhs,
                                    const Number& rhs) noexcept {
    return std::visit(
        [](auto lhs_val, auto rhs_val) -> absl::StatusOr<Number> {
          if constexpr (std::is_floating_point_v<decltype(lhs_val)> ||
                        std::is_floating_point_v<decltype(rhs_val)>) {
            auto divisor = static_cast<double>(rhs_val);
            if (divisor == 0.0) {
              return absl::InvalidArgumentError("Modulo by zero");
            }
            return Number(std::fmod(static_cast<double>(lhs_val), divisor));
          } else {
            if (rhs_val == 0) {
              return absl::InvalidArgumentError("Modulo by zero");
            }
            return Number(lhs_val % rhs_val);
          }
        },
        lhs.value_, rhs.value_);
  }

 private:
  std::variant<int64_t, uint64_t, double> value_;
};

/**
 * Resolves arguments and converts them to a vector of Number.
 * Handles both single values and arrays of values.
 */
absl::StatusOr<std::vector<Number>> GetNumbers(const JsonLogic& eval,
                                               const nlohmann::json& values,
                                               const nlohmann::json& data,
                                               std::string_view op_name) {
  auto resolved_res = eval.Apply(values, data);
  if (!resolved_res.ok()) return resolved_res.status();
  const auto& resolved = *resolved_res;

  std::vector<Number> nums;
  if (resolved.is_array()) {
    nums.reserve(resolved.size());
    for (const auto& item : resolved) {
      if (!item.is_number()) {
        return absl::InvalidArgumentError(std::string(op_name) +
                                          " with non-numeric argument");
      }
      nums.emplace_back(item);
    }
  } else if (resolved.is_number()) {
    nums.emplace_back(resolved);
  } else if (!resolved.is_null()) {
    return absl::InvalidArgumentError(std::string(op_name) +
                                      " with non-numeric argument");
  }
  return nums;
}

}  // namespace

absl::StatusOr<nlohmann::json> Min(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data) {
  auto nums_res = GetNumbers(eval, values, data, "min");
  if (!nums_res.ok()) return nums_res.status();
  if (nums_res->empty()) return nullptr;

  return std::min_element(nums_res->begin(), nums_res->end())->ToJson();
}

absl::StatusOr<nlohmann::json> Max(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data) {
  auto nums_res = GetNumbers(eval, values, data, "max");
  if (!nums_res.ok()) return nums_res.status();
  if (nums_res->empty()) return nullptr;

  return std::max_element(nums_res->begin(), nums_res->end())->ToJson();
}

absl::StatusOr<nlohmann::json> Add(const JsonLogic& eval,
                                   const nlohmann::json& values,
                                   const nlohmann::json& data) {
  auto nums_res = GetNumbers(eval, values, data, "add");
  if (!nums_res.ok()) return nums_res.status();
  if (nums_res->empty()) return 0;

  return std::accumulate(nums_res->begin() + 1, nums_res->end(), (*nums_res)[0],
                         Number::Add)
      .ToJson();
}

absl::StatusOr<nlohmann::json> Subtract(const JsonLogic& eval,
                                        const nlohmann::json& values,
                                        const nlohmann::json& data) {
  auto nums_res = GetNumbers(eval, values, data, "subtract");
  if (!nums_res.ok()) return nums_res.status();

  if (nums_res->size() == 1) {
    return Number::Sub(Number(int64_t{0}), (*nums_res)[0]).ToJson();
  }
  if (nums_res->size() == 2) {
    return Number::Sub((*nums_res)[0], (*nums_res)[1]).ToJson();
  }
  return absl::InvalidArgumentError(
      "Subtract requires exactly one or two arguments");
}

absl::StatusOr<nlohmann::json> Multiply(const JsonLogic& eval,
                                        const nlohmann::json& values,
                                        const nlohmann::json& data) {
  auto nums_res = GetNumbers(eval, values, data, "multiply");
  if (!nums_res.ok()) return nums_res.status();
  if (nums_res->empty()) return 1;

  return std::accumulate(nums_res->begin() + 1, nums_res->end(), (*nums_res)[0],
                         Number::Mul)
      .ToJson();
}

absl::StatusOr<nlohmann::json> Divide(const JsonLogic& eval,
                                      const nlohmann::json& values,
                                      const nlohmann::json& data) {
  auto nums_res = GetNumbers(eval, values, data, "/");
  if (!nums_res.ok()) return nums_res.status();
  if (nums_res->size() != 2) {
    return absl::InvalidArgumentError("Division requires two arguments");
  }

  auto result_res = Number::Div((*nums_res)[0], (*nums_res)[1]);
  if (!result_res.ok()) return result_res.status();
  return result_res->ToJson();
}

absl::StatusOr<nlohmann::json> Modulo(const JsonLogic& eval,
                                      const nlohmann::json& values,
                                      const nlohmann::json& data) {
  auto nums_res = GetNumbers(eval, values, data, "%");
  if (!nums_res.ok()) return nums_res.status();
  if (nums_res->size() != 2) {
    return absl::InvalidArgumentError("Modulo requires two arguments");
  }

  auto result_res = Number::Mod((*nums_res)[0], (*nums_res)[1]);
  if (!result_res.ok()) return result_res.status();
  return result_res->ToJson();
}

absl::StatusOr<nlohmann::json> LessThan(const JsonLogic& eval,
                                        const nlohmann::json& values,
                                        const nlohmann::json& data) {
  auto nums_res = GetNumbers(eval, values, data, "<");
  if (!nums_res.ok()) return nums_res.status();

  if (nums_res->size() == 2) return (*nums_res)[0] < (*nums_res)[1];
  if (nums_res->size() == 3) {
    return (*nums_res)[0] < (*nums_res)[1] && (*nums_res)[1] < (*nums_res)[2];
  }
  return absl::InvalidArgumentError("Comparison requires 2 or 3 arguments");
}

absl::StatusOr<nlohmann::json> LessThanOrEqual(const JsonLogic& eval,
                                               const nlohmann::json& values,
                                               const nlohmann::json& data) {
  auto nums_res = GetNumbers(eval, values, data, "<=");
  if (!nums_res.ok()) return nums_res.status();

  auto is_lte = [](const Number& first, const Number& second) {
    return first < second || first == second;
  };
  if (nums_res->size() == 2) return is_lte((*nums_res)[0], (*nums_res)[1]);
  if (nums_res->size() == 3) {
    return is_lte((*nums_res)[0], (*nums_res)[1]) &&
           is_lte((*nums_res)[1], (*nums_res)[2]);
  }
  return absl::InvalidArgumentError("Comparison requires 2 or 3 arguments");
}

absl::StatusOr<nlohmann::json> GreaterThan(const JsonLogic& eval,
                                           const nlohmann::json& values,
                                           const nlohmann::json& data) {
  auto nums_res = GetNumbers(eval, values, data, ">");
  if (!nums_res.ok()) return nums_res.status();

  if (nums_res->size() == 2) return (*nums_res)[1] < (*nums_res)[0];
  return absl::InvalidArgumentError("GreaterThan requires 2 arguments");
}

absl::StatusOr<nlohmann::json> GreaterThanOrEqual(const JsonLogic& eval,
                                                  const nlohmann::json& values,
                                                  const nlohmann::json& data) {
  auto nums_res = GetNumbers(eval, values, data, ">=");
  if (!nums_res.ok()) return nums_res.status();

  if (nums_res->size() == 2) {
    return (*nums_res)[1] < (*nums_res)[0] || (*nums_res)[0] == (*nums_res)[1];
  }
  return absl::InvalidArgumentError("GreaterThanOrEqual requires 2 arguments");
}

}  // namespace json_logic::ops
