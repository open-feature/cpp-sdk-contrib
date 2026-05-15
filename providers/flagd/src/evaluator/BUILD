load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "datalogic_engine",
    srcs = ["datalogic_engine.cpp"],
    hdrs = ["datalogic_engine.h"],
    include_prefix = "flagd/evaluator",
    strip_include_prefix = "",
    visibility = ["//visibility:public"],
    deps = [
        "//providers/flagd/third_party/datalogic:datalogic_c",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "evaluator",
    srcs = ["evaluator.cpp"],
    hdrs = ["evaluator.h"],
    include_prefix = "flagd/evaluator",
    strip_include_prefix = "",
    visibility = ["//visibility:public"],
    deps = [
        ":datalogic_engine",
        "//providers/flagd/src/sync",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/strings",
        "@nlohmann_json//:json",
        "@openfeature_cpp_sdk//openfeature",
        "@openfeature_cpp_sdk//openfeature:evaluation_context",
    ],
)
