#ifndef PROVIDERS_FLAGD_SRC_EVALUATOR_DATALOGIC_ENGINE_H_
#define PROVIDERS_FLAGD_SRC_EVALUATOR_DATALOGIC_ENGINE_H_

#include <datalogic.h>

#include <nlohmann/json.hpp>

#include "absl/status/statusor.h"

namespace flagd {

// RAII wrapper around the datalogic-rs C engine handle. The underlying
// engine is `Send + Sync` (Arc-wrapped Rust handle); one instance can be
// shared across threads.
class DatalogicEngine {
 public:
  DatalogicEngine();
  ~DatalogicEngine();

  DatalogicEngine(const DatalogicEngine&) = delete;
  DatalogicEngine& operator=(const DatalogicEngine&) = delete;
  DatalogicEngine(DatalogicEngine&&) = delete;
  DatalogicEngine& operator=(DatalogicEngine&&) = delete;

  // One-shot evaluate: serializes `rule` and `data` to JSON strings,
  // calls `datalogic_engine_apply`, parses and returns the result.
  // Returns absl::InternalError carrying the thread-local
  // `datalogic_last_error_*` context on parse or evaluation failure.
  //
  // Performance: every call re-serializes the rule and re-parses it
  // inside datalogic. For hot paths, the C ABI also exposes
  // `datalogic_engine_compile` + `datalogic_rule_evaluate`
  // (compile-once, evaluate-many) which this wrapper could cache
  // behind. Deferred until profiling motivates it.
  absl::StatusOr<nlohmann::json> Apply(const nlohmann::json& rule,
                                       const nlohmann::json& data) const;

 private:
  datalogic_engine* engine_;
};

}  // namespace flagd

#endif  // PROVIDERS_FLAGD_SRC_EVALUATOR_DATALOGIC_ENGINE_H_
