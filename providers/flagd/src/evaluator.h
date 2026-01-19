#ifndef PROVIDERS_FLAGD_SRC_EVALUATOR_H_
#define PROVIDERS_FLAGD_SRC_EVALUATOR_H_

#include <openfeature/evaluation_context.h>
#include <openfeature/resolution_details.h>

#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>

namespace flagd {

class FlagSync;

class Evaluator {
 public:
  virtual ~Evaluator() = default;

  virtual std::unique_ptr<openfeature::BoolResolutionDetails> ResolveBoolean(
      std::string_view flag_key, bool default_value,
      const openfeature::EvaluationContext& ctx) = 0;

  // TODO: Add ResolveString
  // TODO: Add ResolveInt
  // TODO: Add ResolveFloat
  // TODO: Add ResolveObject
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
};

}  // namespace flagd

#endif  // PROVIDERS_FLAGD_SRC_EVALUATOR_H_
