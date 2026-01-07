#ifndef CPP_SDK_FLAGD_EVALUATOR_EVALUATOR_H_
#define CPP_SDK_FLAGD_EVALUATOR_EVALUATOR_H_

#include <functional>
#include <nlohmann/json.hpp>
#include <string>

#include "absl/container/flat_hash_map.h"

namespace flagd {

class Evaluator {
 public:
  // You can assume that the second parameter will be an array
  using OpFunc = std::function<nlohmann::json(
      const Evaluator&, const nlohmann::json&, const nlohmann::json&)>;

  Evaluator();
  ~Evaluator() = default;

  nlohmann::json Evaluate(const nlohmann::json& logic,
                          const nlohmann::json& data) const;

  void RegisterOperation(const std::string& operation, OpFunc func);

 private:
  nlohmann::json EvaluateOp(const std::string& operation,
                            const nlohmann::json& values,
                            const nlohmann::json& data) const;

  absl::flat_hash_map<std::string, OpFunc> operations_;
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_EVALUATOR_EVALUATOR_H_
