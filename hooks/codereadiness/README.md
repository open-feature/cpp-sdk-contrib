# Code Readiness Hook

The `codereadiness` hook allows controlling feature flag evaluation based on the version of the application code.
It does this by comparing the current application version with a required minimum version specified in the flag's metadata.
If the comparison fails (i.e., the application version is lower than the required version), the hook throws an exception, causing the flag evaluation to resolve to its default value.

## Setup

First, add the hook as a dependency of your Bazel target:

```python
cc_binary(
    name = "my_app",
    srcs = ["main.cpp"],
    deps = [
        "@cpp_sdk_contrib//hooks/codereadiness/src:codereadiness",
        "@openfeature_cpp_sdk//openfeature:openfeature_api",
    ],
)
```

Then, include the OpenFeature SDK and the code readiness hook:

```cpp
#include "codereadiness/codereadiness.h"
#include "openfeature/openfeature_api.h"
```

Finally, create the hook with the current version of the application code and register it:

```cpp
// current_version is the current version of the code, which can be retrieved
// from environment variables, build tags, or configuration files.
std::string current_version = "1.4.2";

absl::StatusOr<std::shared_ptr<codereadiness::CodeReadinessHook>> hook =
    codereadiness::CodeReadinessHook::Create(current_version);
if (!hook.ok()) {
  // Handle the error, e.g. empty current_version.
  LOG(ERROR) << "Failed to create code readiness hook: " << hook.status();
  return;
}

// Register the hook globally (it can also be registered on a specific client
// with `client->AddHook(*hook)`).
openfeature::OpenFeatureAPI::GetInstance().AddHook(*hook);
```

`Create` returns an error when:

* `current_version` is empty,
* the metadata key option is empty,
* strict validation is enabled and the comparator fails to initialize with
  `current_version` (e.g. it's not a valid semver).

## How It Works

1. The hook runs during the **After** phase of flag evaluation.
2. It extracts the metadata associated with the evaluated flag.
3. It looks for a specific metadata key (by default, `minCodeVersion`). The metadata value must be a **string**.
4. If found, it compares the current application version against the required minimum version using the configured comparator (by default, a semver comparison).
5. If the current version is **lower** than the required version, it throws a `std::runtime_error`. This triggers the OpenFeature SDK's fallback mechanism, returning the flag's **default value** to the caller.

For example, given the following flag metadata (e.g. in a flagd configuration):

```json
{
  "flags": {
    "new-checkout": {
      "state": "ENABLED",
      "variants": { "on": true, "off": false },
      "defaultVariant": "on",
      "metadata": { "minCodeVersion": "1.5.0" }
    }
  }
}
```

an application created with `current_version = "1.4.2"` evaluates `new-checkout` to the default value passed by the caller, while an application with `current_version = "1.5.0"` or higher evaluates it to `true`.

## Options

The behavior of the hook can be customized by passing a `CodeReadinessHookOptions` struct to `Create`:

| Field                          | Type                                 | Default                     | Description                                                            |
| ------------------------------ | ------------------------------------ | --------------------------- | ---------------------------------------------------------------------- |
| `strict_validation`            | `bool`                               | `false`                     | Fail evaluation when version data is missing or invalid.               |
| `min_version_metadata_key` | `std::string`                        | `"minCodeVersion"`          | Flag metadata key holding the required minimum version. Must not be empty. |
| `comparator`                   | `std::unique_ptr<VersionComparator>` | `nullptr` (`SemVerComparator`) | Custom version comparison logic.                                    |

### Strict Validation

By default, the hook will **not** fail if the version values are missing, empty or have an invalid version format; it logs the reason and skips the validation. To enforce validation and fail the evaluation when version values are missing or invalid, set `strict_validation` to `true`.

```cpp
codereadiness::CodeReadinessHookOptions options;
options.strict_validation = true;

absl::StatusOr<std::shared_ptr<codereadiness::CodeReadinessHook>> hook =
    codereadiness::CodeReadinessHook::Create(current_version,
                                             std::move(options));
```

The table below summarizes the behavior in both modes:

| Situation                                              | `strict_validation = false`    | `strict_validation = true` |
| ------------------------------------------------------ | ------------------------------ | -------------------------- |
| `current_version` is invalid (at `Create`)             | Hook created, checks skipped   | `Create` returns an error  |
| Flag metadata is empty                                 | Skip                           | Evaluation fails           |
| Metadata key is missing                                | Skip                           | Evaluation fails           |
| Metadata value is not a **string** or is **empty**             | Skip                           | Evaluation fails           |
| Comparator returns an error     | Skip                           | Evaluation fails           |
| Current version **lower** than the required version    | Evaluation fails               | Evaluation fails           |
| Current version greater than or equal to the required version | Pass                           | Pass                       |

### Custom Metadata Key

To configure the hook to look for a key other than the default `"minCodeVersion"` in the flag's metadata, set `min_version_metadata_key`.

```cpp
codereadiness::CodeReadinessHookOptions options;
options.min_version_metadata_key = "requiredAppVersion";

absl::StatusOr<std::shared_ptr<codereadiness::CodeReadinessHook>> hook =
    codereadiness::CodeReadinessHook::Create(current_version,
                                             std::move(options));
```

### Custom Comparator

By default, the hook uses `SemVerComparator`, which implements [Semantic Versioning 2.0.0](https://semver.org/) precedence. It accepts an optional leading `v`/`V` (e.g. `v1.2.3`/`V1.2.3`) and the `MAJOR` and `MAJOR.MINOR` shorthands (e.g. `2` is treated as `2.0.0`); build metadata (`+build`) is ignored when comparing. Version components must fit in a 64-bit unsigned integer.

If the application uses a different versioning scheme (such as date-based versioning, revision numbers, or custom build numbers), a custom comparator implementing the `VersionComparator` interface can be provided using the `comparator` field:

```cpp
#include "codereadiness/version_comparator.h"

// Compares plain build numbers, e.g. "1042" >= "1000".
class BuildNumberComparator final : public codereadiness::VersionComparator {
 public:
  // Called once by `Create` with the application's current version.
  absl::Status Initialize(std::string_view current_version) override {
    if (!absl::SimpleAtoi(current_version, &current_)) {
      return absl::InvalidArgumentError("invalid build number");
    }
    initialized_ = true;
    return absl::OkStatus();
  }

  // Returns true if the current version >= required_version. Called
  // concurrently from different flag evaluations, so it must be thread-safe.
  absl::StatusOr<bool> Compare(
      std::string_view required_version) const override {
    if (!initialized_) {
      return absl::FailedPreconditionError("invalid current build number");
    }
    uint64_t required = 0;
    if (!absl::SimpleAtoi(required_version, &required)) {
      return absl::InvalidArgumentError("invalid required build number");
    }
    return current_ >= required;
  }

 private:
  uint64_t current_ = 0;
  bool initialized_ = false;
};

codereadiness::CodeReadinessHookOptions options;
options.comparator = std::make_unique<BuildNumberComparator>();

absl::StatusOr<std::shared_ptr<codereadiness::CodeReadinessHook>> hook =
    codereadiness::CodeReadinessHook::Create("1042", std::move(options));
```

### Logging

The hook logs skipped validations and an invalid current version using Abseil verbose logging at level 1 (`VLOG(1)`). To see these messages, enable verbose logging, for example with `--v=1` or `--vmodule=codereadiness=1`.
