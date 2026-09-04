#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

#include "openfeature/error_code.h"
#include "openfeature/general_flag_evaluation_details.h"
#include "openfeature/reason.h"
#include "openfeature/value.h"

namespace openfeature::contrib::flagd::test {

extern std::string g_current_selector;

std::string ReasonToString(openfeature::Reason reason);
std::string ErrorCodeToString(openfeature::ErrorCode error_code);
void RecordEvaluationDetails(
    const openfeature::GeneralFlagEvaluationDetails& details);
openfeature::Value JsonToValue(const nlohmann::json& json_val);
nlohmann::json ValueToJson(const openfeature::Value& val);

}  // namespace openfeature::contrib::flagd::test
