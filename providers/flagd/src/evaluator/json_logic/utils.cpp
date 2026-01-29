#include "utils.h"

#include <string>

namespace json_logic::ops {

bool Truthy(const nlohmann::json& value) {
  using JsonType = nlohmann::json::value_t;

  switch (value.type()) {
    case JsonType::null:
      return false;

    case JsonType::boolean:
      return value.get<bool>();

    case JsonType::number_integer:
    case JsonType::number_unsigned:
      return value.get<int64_t>() != 0;

    case JsonType::number_float:
      return value.get<double>() != 0.0;

    case JsonType::string:
      return !value.get<std::string>().empty();

    case JsonType::array:
      return !value.empty();

    case JsonType::object:
    default:
      // Objects and any other types are considered truthy.
      return true;
  }
}

}  // namespace json_logic::ops
