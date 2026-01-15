#include "sync.h"

#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <thread>

#include "absl/status/status.h"
#include "embedded_schemas.h"
#include "flagd/configuration.h"
#include "flagd/sync/v1/sync.grpc.pb.h"

using ::flagd::sync::v1::FlagSyncService;
using ::flagd::sync::v1::SyncFlagsRequest;
using ::flagd::sync::v1::SyncFlagsResponse;
using Json = nlohmann::json;
using nlohmann::json_schema::json_validator;

namespace flagd {

namespace {
void Loader(const nlohmann::json_uri& uri, Json& schema) {
  std::string uri_str = uri.to_string();

  if (uri_str.find("/flagd.json") != std::string::npos) {
    schema = Json::parse(schema::FlagdSchema);
  } else if (uri_str.find("/flags.json") != std::string::npos) {
    schema = Json::parse(schema::FlagsSchema);
  } else if (uri_str.find("/targeting.json") != std::string::npos) {
    schema = Json::parse(schema::TargetingSchema);
  } else {
    // TODO(#10): We should log an error here
  }
}
}  // namespace

struct FlagSync::Validator {
  json_validator validator;

  Validator() : validator(Loader) {
    // Initialize with the root schema. This implicitly triggers the Loader if
    // the root schema has $refs to other URIs
    validator.set_root_schema(Json::parse(schema::FlagdSchema));
  }

  bool Validate(const Json& json) const {
    try {
      validator.validate(json);
      return true;
    } catch (const std::exception& e) {
      return false;
    }
  }
};

FlagSync::FlagSync() : validator_(std::make_unique<Validator>()) {
  current_flags_ = std::make_shared<nlohmann::json>(nlohmann::json::object());
}

FlagSync::~FlagSync() = default;

void FlagSync::UpdateFlags(const nlohmann::json& new_json) {
  if (validator_) {
    if (!validator_->Validate(new_json)) {
      // Validation failed, do not update flags.
      // TODO(#10): We should log about it
      return;
    }
  }

  auto new_snapshot = std::make_shared<const nlohmann::json>(new_json["flags"]);
  {
    std::scoped_lock lock(flags_mutex_);
    current_flags_ = std::move(new_snapshot);
  }
}

std::shared_ptr<const nlohmann::json> FlagSync::GetFlags() const {
  std::scoped_lock lock(flags_mutex_);
  return current_flags_;
}

GrpcSync::GrpcSync(FlagdProviderConfig config) : config_(std::move(config)) {}

GrpcSync::~GrpcSync() { GrpcSync::Shutdown().IgnoreError(); }

absl::Status GrpcSync::Init(const openfeature::EvaluationContext& ctx) {
  std::string target = config_.GetEffectiveTargetUri();
  absl::StatusOr<std::shared_ptr<grpc::ChannelCredentials>> creds =
      config_.GetEffectiveCredentials();

  if (!creds.ok()) return creds.status();

  channel_ = grpc::CreateChannel(target, *creds);
  stub_ = FlagSyncService::NewStub(channel_);

  shutdown_requested_ = false;
  background_thread_ = std::thread(&GrpcSync::WaitForUpdates, this);
  return absl::OkStatus();
}

absl::Status GrpcSync::Shutdown() {
  if (shutdown_requested_.exchange(true)) return absl::OkStatus();

  {
    std::scoped_lock lock(connection_mutex_);
    if (context_) context_->TryCancel();
  }

  if (background_thread_.joinable()) {
    background_thread_.join();
  }

  return absl::OkStatus();
}

void GrpcSync::WaitForUpdates() {
  // TODO(#12) Add automatic reconection with exponential backoff
  SyncFlagsRequest request;

  if (config_.GetProviderId().has_value() &&
      !config_.GetProviderId()->empty()) {
    request.set_provider_id(config_.GetProviderId()->data());
  }

  {
    std::scoped_lock lock(connection_mutex_);
    if (shutdown_requested_) return;
    context_ = std::make_unique<grpc::ClientContext>();
  }

  auto reader = stub_->SyncFlags(context_.get(), request);
  SyncFlagsResponse response;

  while (reader->Read(&response)) {
    if (shutdown_requested_) break;

    try {
      Json raw = Json::parse(response.flag_configuration());

      UpdateFlags(raw);
    } catch (const std::exception& e) {
      continue;  // TODO(#10): We should log an error here
    }
  }

  {
    std::scoped_lock lock(connection_mutex_);
    context_.reset();
  }
}

}  // namespace flagd