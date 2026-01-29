load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "json_logic",
    srcs = [
        "json_logic.cpp",
        "array.cpp",
        "data.cpp",
        "logic.cpp",
        "numeric.cpp",
        "string_ops.cpp",
        "utils.cpp",
    ],
    hdrs = [
        "json_logic.h",
        "array.h",
        "data.h",
        "logic.h",
        "numeric.h",
        "string_ops.h",
        "utils.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/container:flat_hash_map",
        "@nlohmann_json//:json",
    ],
)