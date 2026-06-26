load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_test")

SRCS = [
    "steps.cpp",
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
    # TODO(#91): This tag disables those tests from github action check
    # We should remove it once all tests will be passing
    tags = ["manual"],
    deps = DEPS,
)
