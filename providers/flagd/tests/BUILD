load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "provider_test",
    srcs = ["provider_test.cpp"],
    deps = [
        "//providers/flagd/src:flagd_provider",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "sync_test",
    srcs = ["sync_test.cpp"],
    deps = [
        "//providers/flagd/src/sync",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)
