#include "json_logic.h"

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "array.h"
#include "data.h"
#include "logic.h"
#include "numeric.h"
#include "string_ops.h"

namespace json_logic {

JsonLogic::JsonLogic() {
  RegisterOperation("var", ops::Var);
  RegisterOperation("missing", ops::Missing);
  RegisterOperation("missing_some", ops::MissingSome);

  RegisterOperation("and", ops::And);
  RegisterOperation("or", ops::Or);
  RegisterOperation("!", ops::Not);
  RegisterOperation("!!", ops::DoubleNegation);
  RegisterOperation("if", ops::If);
  RegisterOperation("?:", ops::If);
  RegisterOperation("===", ops::StrictEquals);
  RegisterOperation("!==", ops::StrictNotEquals);

  RegisterOperation("+", ops::Add);
  RegisterOperation("-", ops::Subtract);
  RegisterOperation("*", ops::Multiply);
  RegisterOperation("/", ops::Divide);
  RegisterOperation("%", ops::Modulo);
  RegisterOperation("min", ops::Min);
  RegisterOperation("max", ops::Max);
  RegisterOperation("<", ops::LessThan);
  RegisterOperation("<=", ops::LessThanOrEqual);
  RegisterOperation(">", ops::GreaterThan);
  RegisterOperation(">=", ops::GreaterThanOrEqual);

  RegisterOperation("cat", ops::Cat);
  RegisterOperation("substr", ops::Substr);
  RegisterOperation("in", ops::In);

  RegisterOperation("merge", ops::Merge);
}

void JsonLogic::RegisterOperation(std::string_view operation,
                                  const OpFunc& func) {
  operations_.emplace(operation, func);
}

absl::StatusOr<nlohmann::json> JsonLogic::Apply(
    const nlohmann::json& logic, const nlohmann::json& data) const {
  if (logic.is_primitive()) {
    return logic;
  }

  if (logic.is_array()) {
    nlohmann::json result = nlohmann::json::array();
    for (const auto& item : logic) {
      auto applied = Apply(item, data);
      if (!applied.ok()) {
        return applied.status();
      }
      result.push_back(applied.value());
    }
    return result;
  }

  if (logic.is_object()) {
    if (logic.empty()) {
      return logic;
    }

    // There is inconsistency between JsonLogic implementations.
    // When object have more than one key, some execute only the first one,
    // and some treat it as an invalid rule and treat it as data, returning the
    // whole object. Here we went with the first option.
    auto iter = logic.begin();
    std::string const& operation = iter.key();

    absl::StatusOr<nlohmann::json> operation_result =
        ApplyOp(operation, iter.value(), data);

    if (!operation_result.ok() &&
        operation_result.status().code() == absl::StatusCode::kNotFound) {
      return logic;
    }

    return operation_result;
  }

  return nlohmann::json();
}

absl::StatusOr<nlohmann::json> JsonLogic::ApplyOp(
    const std::string& operation, const nlohmann::json& values,
    const nlohmann::json& data) const {
  auto iter = operations_.find(operation);
  if (iter != operations_.end()) {
    return iter->second(*this, values, data);
  }

  return absl::NotFoundError("Unknown operation: " + operation);
}

}  // namespace json_logic
