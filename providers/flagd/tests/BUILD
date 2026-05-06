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

cc_test(
    name = "file_sync_test",
    srcs = ["file_sync_test.cpp"],
    deps = [
        "//providers/flagd/src:flagd_configuration",
        "//providers/flagd/src/sync/file:file_sync",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
        "@openfeature_cpp_sdk//openfeature:evaluation_context",
    ],
)

cc_test(
    name = "configuration_test",
    srcs = ["configuration_test.cpp"],
    deps = [
        "//providers/flagd/src:flagd_configuration",
        "@abseil-cpp//absl/strings",
        "@googletest//:gtest_main",
    ],
)
