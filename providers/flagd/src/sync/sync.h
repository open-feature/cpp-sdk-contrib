#ifndef CPP_SDK_FLAGD_SYNC_H_
#define CPP_SDK_FLAGD_SYNC_H_

#include <memory>
#include <nlohmann/json.hpp>
#include <shared_mutex>

#include "absl/status/status.h"
#include "openfeature/evaluation_context.h"

namespace flagd {

class FlagSync {
 public:
  FlagSync();
  virtual ~FlagSync();

  virtual absl::Status Init(const openfeature::EvaluationContext& ctx) = 0;
  virtual absl::Status Shutdown() = 0;

  std::shared_ptr<const nlohmann::json> GetFlags() const;
  std::shared_ptr<const nlohmann::json> GetMetadata() const;

 protected:
  void UpdateFlags(const nlohmann::json& new_json);
  void ClearFlags();

 private:
  mutable std::shared_mutex flags_mutex_;
  std::shared_ptr<const nlohmann::json> current_flags_;
  std::shared_ptr<const nlohmann::json> global_metadata_;

  class Validator;
  std::unique_ptr<Validator> validator_;
};

}  // namespace flagd

#endif  // CPP_SDK_FLAGD_SYNC_H_
