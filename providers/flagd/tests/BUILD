load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "openfeature",
    srcs = ["smoke/openfeature.cpp"],
    deps = [
        "//providers/flagd:flagd_configuration",
        "//providers/flagd:flagd_provider",
        "@googletest//:gtest_main",
        "@openfeature_cpp_sdk//src:openfeature_cpp_sdk",
    ],
)
