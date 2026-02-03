#ifndef CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_JSON_LOGIC_H_
#define CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_JSON_LOGIC_H_

#include <functional>
#include <nlohmann/json.hpp>
#include <string>

#include "absl/container/flat_hash_map.h"

namespace json_logic {

class JsonLogic {
 public:
  // The second parameter will be an array.
  using OpFunc = std::function<nlohmann::json(
      const JsonLogic&, const nlohmann::json&, const nlohmann::json&)>;

  JsonLogic();
  ~JsonLogic() = default;

  nlohmann::json Apply(const nlohmann::json& logic,
                       const nlohmann::json& data) const;

  void RegisterOperation(std::string_view operation, const OpFunc& func);

 private:
  nlohmann::json ApplyOp(const std::string& operation,
                         const nlohmann::json& values,
                         const nlohmann::json& data) const;

  absl::flat_hash_map<std::string, OpFunc> operations_;
};

}  // namespace json_logic

#endif  // CPP_SDK_FLAGD_EVALUATOR_JSON_LOGIC_JSON_LOGIC_H_
