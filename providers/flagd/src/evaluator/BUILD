load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "evaluator",
    srcs = [
        "evaluator.cpp",
        "ops/array.cpp",
        "ops/data.cpp",
        "ops/logic.cpp",
        "ops/numeric.cpp",
        "ops/string_ops.cpp",
        "ops/utils.cpp",
    ],
    hdrs = [
        "evaluator.h",
        "ops/array.h",
        "ops/data.h",
        "ops/logic.h",
        "ops/numeric.h",
        "ops/string_ops.h",
        "ops/utils.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/container:flat_hash_map",
        "@nlohmann_json//:json",
    ],
)
