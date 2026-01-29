#include "sync.h"

#include <memory>
#include <mutex>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <thread>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
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
  const std::string& path = uri.path();
  auto pos = path.find_last_of('/');
  std::string filename =
      (pos == std::string::npos) ? path : path.substr(pos + 1);
  auto schema_it = schema::schemas.find(filename);
  if (schema_it != schema::schemas.end()) {
    schema = Json::parse(schema_it->second);
  } else {
    // TODO(#10): We should log an error here
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
      // TODO(#10): Log the validation error with details from e.what();
      return false;
    }
  }
};

FlagSync::FlagSync()
    : current_flags_(
          std::make_shared<nlohmann::json>(nlohmann::json::object())),
      validator_(std::make_unique<Validator>()) {}

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

GrpcSync::~GrpcSync() { Shutdown().IgnoreError(); }

absl::Status GrpcSync::Init(const openfeature::EvaluationContext& ctx) {
  std::unique_lock lock(lifecycle_mutex_);

  switch (state_) {
    case State::kReady:
      return absl::OkStatus();
    case State::kInitializing:
      lifecycle_cv_.wait(lock,
                         [this] { return state_ != State::kInitializing; });
      return (state_ == State::kReady) ? absl::OkStatus() : init_result_;
    case State::kShuttingDown:
      lifecycle_cv_.wait(lock,
                         [this] { return state_ == State::kUninitialized; });
      break;
    case State::kUninitialized:
      break;
  }

  state_ = State::kInitializing;
  init_result_ = absl::UnknownError("Initialization incomplete");

  std::string target = config_.GetEffectiveTargetUri();
  auto creds = config_.GetEffectiveCredentials();
  if (!creds.ok()) {
    state_ = State::kUninitialized;
    return creds.status();
  }

  auto channel = grpc::CreateChannel(target, *creds);
  stub_ = FlagSyncService::NewStub(channel);

  context_ = std::make_shared<grpc::ClientContext>();

  try {
    background_thread_ = std::thread(&GrpcSync::WaitForUpdates, this);
  } catch (const std::exception& e) {
    state_ = State::kUninitialized;
    return absl::InternalError(
        absl::StrCat("Failed to spawn thread: ", e.what()));
  }

  lifecycle_cv_.wait(lock, [this] { return state_ != State::kInitializing; });

  if (state_ != State::kReady) {
    if (background_thread_.joinable()) {
      lock.unlock();
      background_thread_.join();
      lock.lock();
    }
    return init_result_;
  }

  return absl::OkStatus();
}

absl::Status GrpcSync::Shutdown() {
  std::unique_lock lock(lifecycle_mutex_);

  if (state_ == State::kUninitialized) {
    return absl::OkStatus();
  }

  if (state_ == State::kShuttingDown) {
    lifecycle_cv_.wait(lock,
                       [this] { return state_ == State::kUninitialized; });
    return absl::OkStatus();
  }

  State previous_state = state_;
  state_ = State::kShuttingDown;

  if (context_) {
    context_->TryCancel();
  }

  lock.unlock();
  if (background_thread_.joinable()) {
    background_thread_.join();
  }
  lock.lock();

  context_.reset();
  stub_.reset();
  state_ = State::kUninitialized;

  if (previous_state == State::kInitializing) {
    init_result_ =
        absl::CancelledError("Shutdown called during initialization");
  }

  lifecycle_cv_.notify_all();

  return absl::OkStatus();
}

void GrpcSync::WaitForUpdates() {
  // TODO(#12) Add automatic reconection with exponential backoff
  SyncFlagsRequest request;

  if (config_.GetProviderId().has_value() &&
      !config_.GetProviderId()->empty()) {
    request.set_provider_id(*config_.GetProviderId());
  }

  std::shared_ptr<grpc::ClientContext> local_ctx;
  {
    std::scoped_lock lock(lifecycle_mutex_);
    local_ctx = context_;
  }

  if (!local_ctx) return;

  auto reader = stub_->SyncFlags(local_ctx.get(), request);
  SyncFlagsResponse response;

  bool first_read = true;
  while (reader->Read(&response)) {
    try {
      Json raw = Json::parse(response.flag_configuration());

      UpdateFlags(raw);

      if (first_read) {
        std::scoped_lock lock(lifecycle_mutex_);
        if (state_ == State::kInitializing) {
          state_ = State::kReady;
          init_result_ = absl::OkStatus();
          lifecycle_cv_.notify_all();
        }
        first_read = false;
      }
    } catch (const std::exception& e) {
      // TODO(#10): We should log an error here
      if (first_read) {
        std::scoped_lock lock(lifecycle_mutex_);
        if (state_ == State::kInitializing) {
          state_ = State::kUninitialized;
          init_result_ =
              absl::InternalError(absl::StrCat("Parse error: ", e.what()));
          lifecycle_cv_.notify_all();
        }
        // If we fail first read, we abort the stream
        return;
      }
    }
  }

  grpc::Status status = reader->Finish();

  {
    std::scoped_lock lock(lifecycle_mutex_);
    if (state_ == State::kInitializing) {
      state_ = State::kUninitialized;
      init_result_ = status.ok()
                         ? absl::InternalError("Stream closed immediately")
                         : absl::InternalError(status.error_message());
      lifecycle_cv_.notify_all();
    }
  }
}

}  // namespace flagd