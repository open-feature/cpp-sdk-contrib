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

cc_test(
    name = "flagd_ops_test",
    srcs = ["flagd_ops_test.cpp"],
    deps = [
        "//providers/flagd/src/evaluator:flagd_ops",
        "//providers/flagd/src/evaluator/json_logic",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "flagd_fractional_op_test",
    srcs = ["flagd_fractional_op_test.cpp"],
    deps = [
        "//providers/flagd/src/evaluator:flagd_ops",
        "//providers/flagd/src/evaluator/json_logic",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)
