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
    name = "datalogic_engine_test",
    srcs = ["datalogic_engine_test.cpp"],
    deps = [
        "//providers/flagd/src/evaluator:datalogic_engine",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)
