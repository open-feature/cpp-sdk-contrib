load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "openfeature",
    srcs = ["openfeature.cpp"],
    deps = [
        "//providers/flagd/src:flagd_configuration",
        "//providers/flagd/src:flagd_provider",
        "@googletest//:gtest_main",
        "@openfeature_cpp_sdk//src:openfeature_cpp_sdk",
    ],
)
