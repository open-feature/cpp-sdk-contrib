load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_test")

SRCS = [
    "steps/config_steps.cpp",
    "steps/context_steps.cpp",
    "steps/evaluation_steps.cpp",
    "steps/flag_steps.cpp",
    "steps/lifecycle_steps.cpp",
    "steps/provider_steps.cpp",
    "steps/step_utils.cpp",
    "steps/step_utils.h",
    "test_env.cpp",
    "test_env.h",
    "test_runner.cpp",
    "test_state.cpp",
    "test_state.h",
]

DATA = [
    "@flagd_binary//:flagd",
    "@flagd_testbed//:features",
    "@flagd_testbed//:flags",
]

DEPS = [
    "//providers/flagd/src:flagd_provider",
    "@bazel_tools//tools/cpp/runfiles",
    "@com_github_grpc_grpc//:grpc++",
    "@cwt_cucumber//:cwt-cucumber",
    "@nlohmann_json//:json",
    "@openfeature_cpp_sdk//openfeature",
    "@openfeature_cpp_sdk//openfeature:openfeature_api",
]

cc_binary(
    name = "gherkin_bin",
    srcs = SRCS,
    copts = ["-std=c++20"],
    data = DATA,
    env = {
        "FLAGD_TEST_FLAGS": "$(locations @flagd_testbed//:flags)",
    },
    deps = DEPS,
)

cc_test(
    name = "gherkin_test",
    srcs = SRCS,
    args = [
        "$(locations @flagd_testbed//:features)",
    ],
    copts = ["-std=c++20"],
    data = DATA,
    env = {
        "FLAGD_TEST_FLAGS": "$(locations @flagd_testbed//:flags)",
    },
    # TODO(#91): This tag disables those tests from github action check
    # We should remove it once all tests will be passing
    tags = ["manual"],
    deps = DEPS,
)
