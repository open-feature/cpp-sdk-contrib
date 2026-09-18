load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

# cwt-cucumber requires C++20. The provider itself targets C++17, so this must
# stay scoped to these targets rather than moving into .bazelrc. copts are not
# propagated across targets in either direction, which is why @cwt_cucumber
# carries its own copy for its own sources.
GHERKIN_COPTS = ["-std=c++20"]

GHERKIN_PLATFORM = [
    "@platforms//os:linux",
    "@platforms//cpu:x86_64",
]

# Feature files the provider can currently satisfy.
#
# Deliberately excluded:
#   connection.feature, events.feature  - need event-handler steps, and the
#                                         provider exposes no event API yet.
#   contextEnrichment.feature           - needs sync-metadata enrichment.
#   sync-payload.feature                - needs sync-metadata enrichment.
#   rpc-caching.feature                 - @rpc only; there is no RPC resolver.
#
# Listing them would add roughly 170 permanently-undefined steps, which buries
# real regressions in noise. Add a file here as soon as its steps exist.
SUPPORTED_FEATURES = [
    "@flagd_testbed//:gherkin/config.feature",
    "@flagd_testbed//:gherkin/disabled.feature",
    "@flagd_testbed//:gherkin/evaluation.feature",
    "@flagd_testbed//:gherkin/metadata.feature",
    "@flagd_testbed//:gherkin/selector.feature",
    "@flagd_testbed//:gherkin/targeting.feature",
]

# flagd binary and fixture flags. Needed in the runfiles of anything that runs
# the suite, and needed as a direct prerequisite of any rule whose `env` uses
# $(rootpaths) on them.
GHERKIN_DATA = [
    "@flagd_binary//:flagd",
    "@flagd_testbed//:flags",
]

# Deliberately NOT built with GHERKIN_COPTS. absl::SourceLocation aliases to
# std::source_location only under C++20, which changes the mangled name of
# every absl error factory. Abseil itself is built at the repo's default
# standard, so a C++20 translation unit calling absl::NotFoundError fails to
# link. This layer pulls in no cucumber headers, so it stays on the default
# standard and can use absl::Status; the C++20 step definitions only ever
# consume the returned Status, never construct one.
cc_library(
    name = "test_env",
    testonly = True,
    srcs = ["test_env.cpp"],
    hdrs = ["test_env.h"],
    data = GHERKIN_DATA,
    tags = ["manual"],
    deps = [
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@bazel_tools//tools/cpp/runfiles",
        "@com_github_grpc_grpc//:grpc++",
        "@nlohmann_json//:json",
    ],
)

# Step definitions register from static initializers that nothing in main()
# references, so without alwayslink the linker drops every object file here and
# every step reports as UNDEFINED.
cc_library(
    name = "gherkin_steps",
    testonly = True,
    srcs = [
        "steps/config_steps.cpp",
        "steps/context_steps.cpp",
        "steps/evaluation_steps.cpp",
        "steps/flag_steps.cpp",
        "steps/lifecycle_steps.cpp",
        "steps/provider_steps.cpp",
        "steps/step_utils.cpp",
        "test_context.cpp",
    ],
    hdrs = [
        "steps/step_utils.h",
        "test_context.h",
    ],
    copts = GHERKIN_COPTS,
    data = GHERKIN_DATA,
    # `bazel build //...` expands to every target in the package, so leaving
    # the library untagged would still make CI fetch the external repos.
    tags = ["manual"],
    deps = [
        ":test_env",
        "//providers/flagd/src:flagd_provider",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@bazel_tools//tools/cpp/runfiles",
        "@com_github_grpc_grpc//:grpc++",
        "@cwt_cucumber//:cwt-cucumber",
        "@nlohmann_json//:json",
        "@openfeature_cpp_sdk//openfeature",
        "@openfeature_cpp_sdk//openfeature:openfeature_api",
    ],
    alwayslink = True,
)

cc_test(
    name = "gherkin_test",
    size = "large",
    srcs = ["test_runner.cpp"],
    args = ["$(rootpath %s)" % feature for feature in SUPPORTED_FEATURES],
    copts = GHERKIN_COPTS,
    data = SUPPORTED_FEATURES + GHERKIN_DATA,
    env = {
        "FLAGD_TEST_FLAGS": "$(rootpaths @flagd_testbed//:flags)",
        # The provider is in-process only; @rpc-tagged scenarios would
        # otherwise run against the sync port and fail for the wrong reason.
        "GHERKIN_TAGS": "@in-process",
    },
    # TODO(#135): drop once the suite is green, so CI can enforce it.
    tags = ["manual"],
    target_compatible_with = GHERKIN_PLATFORM,
    deps = [
        ":gherkin_steps",
        "@cwt_cucumber//:cwt-cucumber",
    ],
)

# Same binary without the Bazel test harness, for running a subset by hand:
#   bazel run //providers/flagd/tests/gherkin:gherkin_bin -- \
#       --tags "@in-process and @targeting" <path/to/feature>
#
# Manual for the same reason as above.
cc_binary(
    name = "gherkin_bin",
    testonly = True,
    srcs = ["test_runner.cpp"],
    copts = GHERKIN_COPTS,
    data = SUPPORTED_FEATURES + GHERKIN_DATA,
    env = {
        "FLAGD_TEST_FLAGS": "$(rootpaths @flagd_testbed//:flags)",
    },
    tags = ["manual"],
    target_compatible_with = GHERKIN_PLATFORM,
    deps = [
        ":gherkin_steps",
        "@cwt_cucumber//:cwt-cucumber",
    ],
)
