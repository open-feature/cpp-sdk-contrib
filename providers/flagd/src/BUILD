load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_python//python:defs.bzl", "py_binary")

cc_library(
    name = "flagd_provider",
    srcs = ["provider.cpp"],
    hdrs = ["provider.h"],
    include_prefix = "flagd",
    visibility = ["//visibility:public"],
    deps = [
        ":flagd_configuration",
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
        ":embedded_schemas",
        ":flagd_configuration",
        "@abseil-cpp//absl/status",
        "@com_github_grpc_grpc//:grpc++",
        "@flagd-schemas//:flagd_sync_cc_grpc",
        "@flagd-schemas//:flagd_sync_cc_proto",
        "@nlohmann_json//:json",
        "@nlohmann_json_schema_validator//:json_schema_validator",
        "@openfeature_cpp_sdk//openfeature",
        "@openfeature_cpp_sdk//openfeature:evaluation_context",
    ],
)

py_binary(
    name = "generate_schema_header_bin",
    srcs = ["generate_schema_header.py"],
    main = "generate_schema_header.py",
)

genrule(
    name = "generate_schema_header",
    srcs = ["@flagd-schemas//:json_schemas"],
    outs = ["embedded_schemas.h"],
    cmd = "$(location :generate_schema_header_bin) --output $@ $(SRCS)",
    tools = [":generate_schema_header_bin"],
)

cc_library(
    name = "embedded_schemas",
    hdrs = ["embedded_schemas.h"],
    includes = ["."],
    visibility = ["//visibility:public"],
)
