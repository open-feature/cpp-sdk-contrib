#ifndef CPP_SDK_FLAGD_PROVIDER_H_
#define CPP_SDK_FLAGD_PROVIDER_H_

#include <openfeature/provider.h>

#include <atomic>
#include <memory>

#include "absl/status/status.h"
#include "flagd/configuration.h"
#include "flagd/evaluator.h"
#include "flagd/sync.h"

namespace flagd {

class FlagdProvider : public openfeature::FeatureProvider {
 public:
  explicit FlagdProvider(FlagdProviderConfig config = FlagdProviderConfig());

  ~FlagdProvider() override;

  openfeature::Metadata GetMetadata() const override;

  absl::Status Init(const openfeature::EvaluationContext& ctx) override;
  absl::Status Shutdown() override;

  std::unique_ptr<openfeature::BoolResolutionDetails> GetBooleanEvaluation(
      std::string_view flag, bool default_value,
      const openfeature::EvaluationContext& ctx) override;

  std::unique_ptr<openfeature::StringResolutionDetails> GetStringEvaluation(
      std::string_view flag, std::string_view default_value,
      const openfeature::EvaluationContext& ctx) override;

  std::unique_ptr<openfeature::IntResolutionDetails> GetIntegerEvaluation(
      std::string_view flag, int64_t default_value,
      const openfeature::EvaluationContext& ctx) override;

  std::unique_ptr<openfeature::DoubleResolutionDetails> GetDoubleEvaluation(
      std::string_view flag, double default_value,
      const openfeature::EvaluationContext& ctx) override;

  std::unique_ptr<openfeature::ObjectResolutionDetails> GetObjectEvaluation(
      std::string_view flag, openfeature::Value default_value,
      const openfeature::EvaluationContext& ctx) override;

 private:
  FlagdProviderConfig configuration_;
  std::shared_ptr<FlagSync> sync_;
  std::unique_ptr<Evaluator> evaluator_;
  std::atomic<bool> is_ready_;
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_PROVIDER_H_
