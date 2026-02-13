load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "json_logic_test",
    srcs = ["json_logic_test.cpp"],
    deps = [
        "//providers/flagd/src/evaluator/json_logic",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "ops_test",
    srcs = ["ops_test.cpp"],
    deps = [
        "//providers/flagd/src/evaluator/json_logic",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "json_logic_official_test",
    srcs = ["json_logic_official_test.cpp"],
    data = ["tests.json"],
    deps = [
        "//providers/flagd/src/evaluator/json_logic",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "json_logic_annotated_test",
    srcs = ["json_logic_annotated_test.cpp"],
    data = ["tests_annotated.json"],
    deps = [
        "//providers/flagd/src/evaluator/json_logic",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)
