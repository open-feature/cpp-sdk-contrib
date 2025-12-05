load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "flagd_provider",
    srcs = ["src/provider.cpp"],
    hdrs = ["include/flagd/provider.h"],
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [
        ":flagd_configuration",
        "@abseil-cpp//absl/status",
        "@openfeature_cpp_sdk//src:openfeature_cpp_sdk",
    ],
)

cc_library(
    name = "flagd_configuration",
    srcs = ["src/configuration.cpp"],
    hdrs = ["include/flagd/configuration.h"],
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/strings",
        "@openfeature_cpp_sdk//src:openfeature_cpp_sdk",
    ],
)
