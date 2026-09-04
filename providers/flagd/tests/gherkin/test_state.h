#pragma once

#include <any>
#include <map>
#include <optional>
#include <string>

#include "flagd/provider.h"
#include "openfeature/error_code.h"
#include "openfeature/flag_metadata.h"
#include "openfeature/reason.h"
#include "openfeature/value.h"

namespace openfeature::contrib::flagd::test {

// Shared context state across Gherkin Given/When/Then steps.
struct TestState {
  std::string cache_type = "disabled";
  std::string selector;
  std::shared_ptr<::flagd::FlagdProvider> provider;
  std::string targeting_key;
  std::map<std::string, std::any> context_attributes;
  std::map<std::string, std::map<std::string, ::openfeature::Value>>
      nested_context_attributes;

  struct {
    std::string flag_key;
    std::string flag_type;
    std::string default_value_str;
    ::openfeature::Value resolved_value;
    std::optional<::openfeature::Reason> reason;
    std::optional<std::string> variant;
    std::optional<::openfeature::ErrorCode> error_code;
    std::optional<std::string> error_message;
    ::openfeature::FlagMetadata flag_metadata;
  } last_eval;

  std::optional<::flagd::FlagdProviderConfig> config;
  bool config_error = false;
  std::vector<std::string> set_env_vars;
  std::map<std::string, std::string> pending_options;
};

extern TestState g_state;

// Resets per-scenario context evaluation state while preserving persistent
// connections.
void ResetTestState();

}  // namespace openfeature::contrib::flagd::test
