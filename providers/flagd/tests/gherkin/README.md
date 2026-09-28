# Gherkin integration tests

Runs the [flagd-testbed](https://github.com/open-feature/flagd-testbed) Gherkin
suite against the C++ flagd provider, using
[cwt-cucumber](https://github.com/ThoSe1990/cwt-cucumber) as the runner.

A real `flagd` binary is downloaded by Bazel and started as a subprocess for
the duration of the run; the tests talk to it over gRPC exactly as a real
application would.

## Running

The target is tagged `manual`, so `bazel test //providers/...` skips it. Run it
explicitly:

```sh
bazel test //providers/flagd/tests/gherkin:gherkin_test --test_output=all
```

To run a subset, use the binary directly:

```sh
# Everything tagged @targeting
bazel run //providers/flagd/tests/gherkin:gherkin_bin -- \
    --tags "@in-process and @targeting" \
    $PWD/bazel-cpp-sdk-contrib/external/+_repo_rules+flagd_testbed/gherkin/targeting.feature

# A single scenario by name
bazel run //providers/flagd/tests/gherkin:gherkin_bin -- \
    --name "Returns metadata" \
    $PWD/bazel-cpp-sdk-contrib/external/+_repo_rules+flagd_testbed/gherkin/metadata.feature
```

`--tags` and `--name` can also be supplied as `GHERKIN_TAGS` and
`GHERKIN_NAME`; command-line flags win over the environment.

> [!NOTE]
> Linux x86_64 only. The `flagd` release archive pinned in `MODULE.bazel` has
> no other platform, so `target_compatible_with` makes these targets *skip*
> silently elsewhere rather than fail.

## Layout

| File | Purpose |
|---|---|
| `test_runner.cpp` | `main()`; normalises arguments and calls `cuke::entry_point` |
| `test_env.{h,cpp}` | Starts/stops flagd, merges the fixture files, resolves runfiles. **Separate target, not built with C++20** — see below |
| `test_context.{h,cpp}` | All mutable test state, plus environment save/restore |
| `steps/flag_steps.cpp` | `a <Type>-flag with key ...` |
| `steps/context_steps.cpp` | `a context containing ...` |
| `steps/provider_steps.cpp` | `a stable flagd provider`, option collection |
| `steps/evaluation_steps.cpp` | `the flag was evaluated with details` and its assertions |
| `steps/config_steps.cpp` | `a config was initialized` and option assertions |
| `steps/lifecycle_steps.cpp` | `BEFORE_ALL` / `AFTER_ALL` / per-scenario reset |
| `steps/step_utils.{h,cpp}` | Conversions, parsing, assertion helpers |

## Three things that will confuse you

**Empty Scenario-Outline cells arrive as four quote characters.**
cwt-cucumber substitutes an empty Examples cell with the literal `""`, which
combines with the quotes already in the step text. `{string}` (`"([^"]*)"`)
cannot match that, so every step whose value may be blank is registered twice —
once normally and once with `GHERKIN_EMPTY_ARG`. Both registrations delegate to
one function. See the comment on `GHERKIN_EMPTY_ARG` in `steps/step_utils.h`.

**Step definitions need `alwayslink`.**
Steps register themselves from static initializers. In a plain `cc_library` the
linker discards every object file that `main()` does not reference, taking the
registrations with it — the binary links cleanly and reports *every* step as
undefined. The `gherkin_steps` target sets `alwayslink = True`.

**C++20 changes Abseil's ABI, so `test_env` is a separate target.**
cwt-cucumber requires C++20, but the provider and all its dependencies — including
Abseil — build at the repository default. `absl::SourceLocation` aliases to
`std::source_location` only under C++20, which changes the mangled name of every
Abseil error factory. A C++20 translation unit calling `absl::NotFoundError` therefore
fails to link:

```
undefined reference to absl::status_internal::MakeErrorImpl<5>(
    string_view, std::source_location)
```

`test_env.{h,cpp}` pulls in no cucumber headers, so it lives in its own `cc_library`
*without* `GHERKIN_COPTS` and can use `absl::Status` normally. Do not add
`copts = GHERKIN_COPTS` to that target.

> [!WARNING]
> The step definitions may only **consume** an `absl::Status` — `.ok()`,
> `.message()`, `operator<<`. Constructing one from a C++20 translation unit will
> not link. `absl::Status` is a single `uintptr_t`, so passing it across the
> boundary is layout-safe, but any Abseil API whose *signature* depends on a C++20
> feature is not usable from `gherkin_steps`.
>
> If a step ever needs to build a `Status`, add a factory to `test_env` and call
> that instead — or move the whole repository to C++20.

## Known gaps

These are real provider gaps, not harness bugs. Scenarios covering them fail or
are excluded on purpose; see the `SUPPORTED_FEATURES` list in `BUILD`.

| Gap | Effect |
|---|---|
| No RPC resolver | `rpc-caching.feature` excluded; `GHERKIN_TAGS` pins the run to `@in-process` |
| No file/offline resolver (TODO #20) | `FlagdProvider` calls `LOG(FATAL)` when `offlineFlagSourcePath` is set, which would abort the whole run |
| No provider events | `connection.feature`, `events.feature` excluded |
| No sync-metadata enrichment | `contextEnrichment.feature`, `sync-payload.feature` excluded |
| No `resolver` / `cache` / `maxCacheSize` in `FlagdProviderConfig` | Those `config.feature` scenarios report as not-implemented |
| `edge-case-flags.json`, `custom-ops.json` rejected by FlagSync (TODO #129) | Fixtures skipped; dependent scenarios fail |

Steps deliberately fail rather than pass when they cannot verify something. A
step that recognises none of its inputs runs zero assertions, and reporting
that as success is how a suite ends up certifying unimplemented behaviour.
