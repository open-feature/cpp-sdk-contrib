load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "grpc_sync",
    srcs = ["grpc_sync.cpp"],
    hdrs = [
        "grpc_sync.h",
    ],
    include_prefix = "flagd/sync/grpc",
    strip_include_prefix = "",
    visibility = ["//visibility:public"],
    deps = [
        "//providers/flagd/src:flagd_configuration",
        "//providers/flagd/src/sync",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/status",
        "@com_github_grpc_grpc//:grpc++",
        "@flagd_schemas//:flagd_sync_cc_grpc",
        "@flagd_schemas//:flagd_sync_cc_proto",
        "@nlohmann_json//:json",
        "@openfeature_cpp_sdk//openfeature",
        "@openfeature_cpp_sdk//openfeature:evaluation_context",
    ],
)
