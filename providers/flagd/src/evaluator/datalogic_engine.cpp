#include "datalogic_engine.h"

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace flagd {

namespace {

std::string LastError() {
  const char* type = datalogic_last_error_type();
  const char* msg = datalogic_last_error_message();
  return absl::StrCat(type ? type : "Unknown", ": ", msg ? msg : "(no detail)");
}

}  // namespace

DatalogicEngine::DatalogicEngine()
    : engine_(datalogic_engine_new(/*templating=*/0)) {}

DatalogicEngine::~DatalogicEngine() { datalogic_engine_free(engine_); }

absl::StatusOr<nlohmann::json> DatalogicEngine::Apply(
    const nlohmann::json& rule, const nlohmann::json& data) const {
  if (engine_ == nullptr) {
    return absl::InternalError("datalogic engine not initialized");
  }
  const std::string rule_str = rule.dump();
  const std::string data_str = data.dump();

  std::unique_ptr<char, decltype(&datalogic_string_free)> result(
      datalogic_engine_apply(engine_, rule_str.c_str(), data_str.c_str()),
      datalogic_string_free);
  if (result == nullptr) {
    return absl::InternalError(LastError());
  }
  try {
    return nlohmann::json::parse(result.get());
  } catch (const nlohmann::json::exception& err) {
    return absl::InternalError(
        absl::StrCat("failed to parse datalogic result: ", err.what()));
  }
}

}  // namespace flagd
