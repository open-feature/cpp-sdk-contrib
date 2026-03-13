load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "provider_test",
    srcs = ["provider_test.cpp"],
    deps = [
        "//providers/flagd/src:flagd_provider",
        "@googletest//:gtest_main",
    ],
)
