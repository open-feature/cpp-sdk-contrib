load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "flagd_provider",
    srcs = ["src/provider.cpp"],
    hdrs = ["include/flagd/provider.h"],
    strip_include_prefix = "include",
    visibility = ["//visibility:public"],
    deps = [
        ":flagd_configuration",
        "@abseil-cpp//absl/status",
        "@openfeature_cpp_sdk//openfeature",
    ],
)

cc_library(
    name = "flagd_configuration",
    srcs = ["src/configuration.cpp"],
    hdrs = ["include/flagd/configuration.h"],
    strip_include_prefix = "include",
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/strings",
        "@com_github_grpc_grpc//:grpc++",
        "@openfeature_cpp_sdk//openfeature",
    ],
)

cc_library(
    name = "sync",
    srcs = ["src/sync.cpp"],
    hdrs = ["include/flagd/sync.h"],
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [
        ":flagd_configuration",
        "@abseil-cpp//absl/status",
        "@com_github_grpc_grpc//:grpc++",
        "@flagd-schemas//:flagd_sync_cc_grpc",
        "@flagd-schemas//:flagd_sync_cc_proto",
        "@nlohmann_json//:json",
        "@openfeature_cpp_sdk//openfeature",
        "@openfeature_cpp_sdk//openfeature:evaluation_context",
    ],
)
