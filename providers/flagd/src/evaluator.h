#ifndef PROVIDERS_FLAGD_SRC_EVALUATOR_H_
#define PROVIDERS_FLAGD_SRC_EVALUATOR_H_

#include <openfeature/evaluation_context.h>
#include <openfeature/resolution_details.h>

#include <memory>
#include <nlohmann/json.hpp>
#include <string_view>

#include "flagd/evaluator/json_logic/json_logic.h"

namespace flagd {

class FlagSync;

class Evaluator {
 public:
  virtual ~Evaluator() = default;

  virtual std::unique_ptr<openfeature::BoolResolutionDetails> ResolveBoolean(
      std::string_view flag_key, bool default_value,
      const openfeature::EvaluationContext& ctx) = 0;

  // TODO: Add other resolve functions (int, object etc.) once SDK defines them
};

class JsonLogicEvaluator : public Evaluator {
 public:
  explicit JsonLogicEvaluator(std::shared_ptr<FlagSync> sync);

  std::unique_ptr<openfeature::BoolResolutionDetails> ResolveBoolean(
      std::string_view flag_key, bool default_value,
      const openfeature::EvaluationContext& ctx) override;

 private:
  template <typename T>
  std::unique_ptr<openfeature::ResolutionDetails<T>> ResolveAny(
      std::string_view flag_key, T default_value,
      const openfeature::EvaluationContext& ctx);

  std::shared_ptr<FlagSync> sync_;
  json_logic::JsonLogic json_logic_;
};

}  // namespace flagd

#endif  // PROVIDERS_FLAGD_SRC_EVALUATOR_H_
