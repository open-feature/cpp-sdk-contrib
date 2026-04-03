load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "fractional_test",
    srcs = ["fractional_test.cpp"],
    data = [
        ":test1000",
    ],
    deps = [
        "//providers/flagd/src:flagd_provider",
        "//providers/flagd/src/sync",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
        "@openfeature_cpp_sdk//openfeature",
        "@openfeature_cpp_sdk//openfeature:openfeature_api",
    ],
)

filegroup(
    name = "test1000",
    srcs = glob(["test1000/**"]),
)
