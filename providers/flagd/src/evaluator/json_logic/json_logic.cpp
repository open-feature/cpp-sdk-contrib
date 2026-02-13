#include "json_logic.h"

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "data.h"
#include "logic.h"

namespace json_logic {

JsonLogic::JsonLogic() {
  RegisterOperation("var", ops::Var);
  RegisterOperation("missing", ops::Missing);
  RegisterOperation("missing_some", ops::MissingSome);

  RegisterOperation("and", ops::And);
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
    nlohmann::json::const_iterator iter = logic.begin();
    std::string const& operation = iter.key();

    absl::StatusOr<nlohmann::json> operation_result =
        ApplyOp(operation, iter.value(), data);

    if (!operation_result.ok()) {
      return logic;
    }
    return operation_result;
  }

  return nlohmann::json{};
}

absl::StatusOr<nlohmann::json> JsonLogic::ApplyOp(
    const std::string& operation, const nlohmann::json& values,
    const nlohmann::json& data) const {
  absl::flat_hash_map<std::string, OpFunc>::const_iterator iter =
      operations_.find(operation);
  if (iter != operations_.end()) {
    return iter->second(*this, values, data);
  }

  return absl::InvalidArgumentError("Unknown operation: " + operation);
}

}  // namespace json_logic
