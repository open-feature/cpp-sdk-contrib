load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "sync",
    srcs = ["sync.cpp"],
    hdrs = ["sync.h"],
    include_prefix = "flagd/sync",
    strip_include_prefix = "",
    visibility = ["//visibility:public"],
    deps = [
        "//providers/flagd/src:flagd_configuration",
        "//providers/flagd/tools:embedded_schemas",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/status",
        "@nlohmann_json//:json",
        "@nlohmann_json_schema_validator//:json_schema_validator",
        "@openfeature_cpp_sdk//openfeature",
        "@openfeature_cpp_sdk//openfeature:evaluation_context",
    ],
)
