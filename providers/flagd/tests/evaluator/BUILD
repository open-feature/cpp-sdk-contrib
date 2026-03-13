load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "evaluator_test",
    srcs = ["evaluator_test.cpp"],
    deps = [
        "//providers/flagd/src/evaluator",
        "//providers/flagd/src/sync",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)
