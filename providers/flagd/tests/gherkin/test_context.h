#pragma once

#include <any>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "flagd/configuration.h"
#include "flagd/provider.h"
#include "openfeature/error_code.h"
#include "openfeature/flag_metadata.h"
#include "openfeature/reason.h"
#include "openfeature/value.h"
#include "providers/flagd/tests/gherkin/steps/step_utils.h"

namespace openfeature::contrib::flagd::test {

// Saves environment variables before a scenario overwrites them and puts the
// originals back afterwards, so the BEFORE hook need not duplicate cleanup.
class ScopedEnv {
 public:
  ScopedEnv() = default;
  ~ScopedEnv();

  ScopedEnv(const ScopedEnv&) = delete;
  ScopedEnv& operator=(const ScopedEnv&) = delete;
  ScopedEnv(ScopedEnv&&) = delete;
  ScopedEnv& operator=(ScopedEnv&&) = delete;

  // Sets `name` to `value`, remembering the prior value on first touch.
  void Set(const std::string& name, const std::string& value);

  // Restores every variable touched since the last call. Idempotent.
  void RestoreAll();

 private:
  std::map<std::string, std::optional<std::string>> saved_;
};

struct PendingEvaluation {
  bool declared = false;
  std::string flag_key;
  FlagType flag_type = FlagType::kBoolean;
  std::string default_value_str;
};

// Separate from PendingEvaluation so a THEN step cannot silently assert
// against a previous scenario's leftovers.
struct EvaluationResult {
  bool recorded = false;
  ::openfeature::Value value;
  std::optional<::openfeature::Reason> reason;
  std::optional<std::string> variant;
  std::optional<::openfeature::ErrorCode> error_code;
  std::optional<std::string> error_message;
  ::openfeature::FlagMetadata flag_metadata;
};

struct ScenarioState {
  std::string selector;
  std::string targeting_key;
  std::map<std::string, std::any> context_attributes;
  std::map<std::string, std::map<std::string, ::openfeature::Value>>
      nested_context_attributes;

  PendingEvaluation pending_eval;
  EvaluationResult last_eval;

  // Applied when "a config was initialized" or a provider step runs.
  std::map<std::string, std::string> pending_options;

  std::optional<::flagd::FlagdProviderConfig> config;
  bool config_error = false;

  ScopedEnv env;
};

// Kept alive across scenarios so each one does not pay for a fresh gRPC
// connection and provider handshake.
struct PersistentState {
  std::shared_ptr<::flagd::FlagdProvider> provider;
  // A scenario asking for a different selector forces a rebuild.
  std::string provider_selector;
  bool provider_registered = false;
};

struct TestContext {
  ScenarioState scenario;
  PersistentState persistent;
};

TestContext& Ctx();

// Restores the environment and clears per-scenario state. Persistent state
// (the cached provider) deliberately survives.
void ResetScenarioState();

}  // namespace openfeature::contrib::flagd::test
