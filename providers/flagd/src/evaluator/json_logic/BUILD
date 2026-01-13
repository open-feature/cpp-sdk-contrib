load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "json_logic",
    srcs = [
        "json_logic.cpp",
        "ops/array.cpp",
        "ops/data.cpp",
        "ops/logic.cpp",
        "ops/numeric.cpp",
        "ops/string_ops.cpp",
        "ops/utils.cpp",
    ],
    hdrs = [
        "json_logic.h",
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
