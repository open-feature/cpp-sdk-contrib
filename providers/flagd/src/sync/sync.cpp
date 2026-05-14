#include "sync.h"

#include <memory>
#include <mutex>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>

#include "absl/log/log.h"
#include "embedded_schemas.h"

using Json = nlohmann::json;
using nlohmann::json_schema::json_validator;

namespace flagd {

namespace {
void Loader(const nlohmann::json_uri& uri, Json& schema) {
  const std::string& path = uri.path();
  auto pos = path.find_last_of('/');
  std::string filename =
      (pos == std::string::npos) ? path : path.substr(pos + 1);
  auto schema_it = schema::schemas.find(filename);
  if (schema_it != schema::schemas.end()) {
    schema = Json::parse(schema_it->second);
  } else {
    LOG(ERROR) << "Schema not found for filename: " << filename;
  }
}
}  // namespace

class FlagSync::Validator {
 public:
  json_validator validator;

  Validator() : validator(Loader) {
    // Initialize with the root schema. This implicitly triggers the Loader if
    // the root schema has $refs to other URIs
    validator.set_root_schema(Json::parse(schema::schemas.at("flagd.json")));
  }

  bool Validate(const Json& json) const {
    try {
      validator.validate(json);
      return true;
    } catch (const std::exception& e) {
      LOG(ERROR) << "Flag configuration validation failed: " << e.what();
      return false;
    }
  }
};

FlagSync::FlagSync()
    : current_flags_(
          std::make_shared<nlohmann::json>(nlohmann::json::object())),
      global_metadata_(
          std::make_shared<nlohmann::json>(nlohmann::json::object())),
      validator_(std::make_unique<Validator>()) {}

FlagSync::~FlagSync() = default;

void FlagSync::UpdateFlags(const nlohmann::json& new_json) {
  if (validator_) {
    if (!validator_->Validate(new_json)) {
      // Validation failed, do not update flags.
      LOG(ERROR) << "Flag configuration validation failed, skipping update.";
      return;
    }
  }

  auto new_flags_snapshot =
      std::make_shared<const nlohmann::json>(new_json["flags"]);
  std::shared_ptr<const nlohmann::json> new_metadata_snapshot;
  if (new_json.contains("metadata")) {
    new_metadata_snapshot =
        std::make_shared<const nlohmann::json>(new_json["metadata"]);
  } else {
    new_metadata_snapshot =
        std::make_shared<const nlohmann::json>(nlohmann::json::object());
  }

  {
    std::scoped_lock lock(flags_mutex_);
    current_flags_ = std::move(new_flags_snapshot);
    global_metadata_ = std::move(new_metadata_snapshot);
  }
}

void FlagSync::ClearFlags() {
  {
    std::scoped_lock lock(flags_mutex_);
    current_flags_ = std::make_shared<nlohmann::json>(nlohmann::json::object());
    global_metadata_ =
        std::make_shared<nlohmann::json>(nlohmann::json::object());
  }
}

std::shared_ptr<const nlohmann::json> FlagSync::GetFlags() const {
  std::shared_lock lock(flags_mutex_);
  return current_flags_;
}

std::shared_ptr<const nlohmann::json> FlagSync::GetMetadata() const {
  std::shared_lock lock(flags_mutex_);
  return global_metadata_;
}

}  // namespace flagd
