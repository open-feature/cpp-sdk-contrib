load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "file_sync",
    srcs = ["file_sync.cpp"],
    hdrs = [
        "file_sync.h",
    ],
    include_prefix = "flagd/sync/file",
    strip_include_prefix = "",
    visibility = ["//visibility:public"],
    deps = [
        "//providers/flagd/src:flagd_configuration",
        "//providers/flagd/src/sync",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@nlohmann_json//:json",
        "@openfeature_cpp_sdk//openfeature",
        "@openfeature_cpp_sdk//openfeature:evaluation_context",
    ],
)
