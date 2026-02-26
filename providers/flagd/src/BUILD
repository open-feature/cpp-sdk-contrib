load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "flagd_provider",
    srcs = ["provider.cpp"],
    hdrs = ["provider.h"],
    include_prefix = "flagd",
    visibility = ["//visibility:public"],
    deps = [
        ":evaluator",
        ":flagd_configuration",
        ":sync",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/status",
        "@openfeature_cpp_sdk//openfeature",
    ],
)

cc_library(
    name = "flagd_configuration",
    srcs = ["configuration.cpp"],
    hdrs = ["configuration.h"],
    include_prefix = "flagd",
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/strings",
        "@com_github_grpc_grpc//:grpc++",
        "@openfeature_cpp_sdk//openfeature",
    ],
)

cc_library(
    name = "sync",
    srcs = ["sync.cpp"],
    hdrs = ["sync.h"],
    include_prefix = "flagd",
    visibility = ["//visibility:public"],
    deps = [
        ":flagd_configuration",
        "//providers/flagd/tools:embedded_schemas",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/status",
        "@com_github_grpc_grpc//:grpc++",
        "@flagd_schemas//:flagd_sync_cc_grpc",
        "@flagd_schemas//:flagd_sync_cc_proto",
        "@nlohmann_json//:json",
        "@nlohmann_json_schema_validator//:json_schema_validator",
        "@openfeature_cpp_sdk//openfeature",
        "@openfeature_cpp_sdk//openfeature:evaluation_context",
    ],
)

cc_library(
    name = "evaluator",
    srcs = ["evaluator.cpp"],
    hdrs = ["evaluator.h"],
    include_prefix = "flagd",
    visibility = ["//visibility:public"],
    deps = [
        ":sync",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/strings",
        "@nlohmann_json//:json",
        "@openfeature_cpp_sdk//openfeature",
        "@openfeature_cpp_sdk//openfeature:evaluation_context",
    ],
)
