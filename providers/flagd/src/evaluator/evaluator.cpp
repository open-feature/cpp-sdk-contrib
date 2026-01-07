#include "evaluator.h"

#include <string>
#include <utility>

#include "ops/data.h"
#include "ops/logic.h"

namespace flagd {

Evaluator::Evaluator() {
  RegisterOperation("var", ops::Var);
  RegisterOperation("and", ops::And);
}

void Evaluator::RegisterOperation(std::string_view operation, OpFunc func) {
  operations_.emplace(operation, std::move(func));
}

nlohmann::json Evaluator::Evaluate(const nlohmann::json& logic,
                                   const nlohmann::json& data) const {
  if (logic.is_primitive()) {
    return logic;
  }

  if (logic.is_array()) {
    nlohmann::json result = nlohmann::json::array();
    for (const auto& item : logic) {
      result.push_back(Evaluate(item, data));
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
    return EvaluateOp(operation, iter.value(), data);
  }

  return {};
}

nlohmann::json Evaluator::EvaluateOp(const std::string& operation,
                                     const nlohmann::json& values,
                                     const nlohmann::json& data) const {
  auto iter = operations_.find(operation);
  if (iter != operations_.end()) {
    // We are ensuring that every operation is receiving array as data, so that
    // the functions won't have to check for that
    if (values.is_array()) {
      return iter->second(*this, values, data);
    }
    return iter->second(*this, nlohmann::json::array({values}), data);
  }

  return {};
}

}  // namespace flagd
