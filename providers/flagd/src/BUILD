load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "flagd_provider",
    srcs = ["provider.cpp"],
    hdrs = ["provider.h"],
    include_prefix = "flagd",
    visibility = ["//visibility:public"],
    deps = [
        ":flagd_configuration",
        "//providers/flagd/src/evaluator",
        "//providers/flagd/src/sync",
        "//providers/flagd/src/sync/file:file_sync",
        "//providers/flagd/src/sync/grpc:grpc_sync",
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
