load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_test")

cc_test(
    name = "openfeature",
    srcs = ["openfeature.cpp"],
    deps = [
        "//providers/flagd/src:flagd_configuration",
        "//providers/flagd/src:flagd_provider",
        "@googletest//:gtest_main",
        "@openfeature_cpp_sdk//openfeature",
    ],
)

cc_test(
    name = "sync",
    srcs = ["sync.cpp"],
    deps = [
        "//providers/flagd/src/sync",
        "//providers/flagd/src/sync/grpc:grpc_sync",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@nlohmann_json//:json",
    ],
)

cc_binary(
    name = "grpc_sync",
    srcs = ["grpc_sync.cpp"],
    deps = [
        "//providers/flagd/src:flagd_configuration",
        "//providers/flagd/src/sync/grpc:grpc_sync",
    ],
)
